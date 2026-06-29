#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/NoExportTypes.h"
#include "UObject/PropertyOptional.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageUFunctionTest,
	"Angelscript.TestModule.Coverage.UFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static UFunction* FindFunctionForTest(UClass* ScriptClass, FName FunctionName)
	{
		if (ScriptClass == nullptr)
			return nullptr;

		return FindGeneratedFunction(ScriptClass, FunctionName);
	}

	static FProperty* FindParameterForTest(UFunction* Function, FName ParameterName)
	{
		if (Function == nullptr)
			return nullptr;

		return FindFProperty<FProperty>(Function, ParameterName);
	}

	static TArray<FProperty*> GetOrderedParameters(UFunction* Function)
	{
		TArray<FProperty*> Parameters;
		if (Function == nullptr)
		{
			return Parameters;
		}

		for (TFieldIterator<FProperty> It(Function); It; ++It)
		{
			FProperty* Property = *It;
			if (Property != nullptr
				&& Property->HasAnyPropertyFlags(CPF_Parm)
				&& !Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				Parameters.Add(Property);
			}
		}

		Parameters.Sort([](const FProperty& Left, const FProperty& Right)
		{
			return Left.GetOffset_ForUFunction() < Right.GetOffset_ForUFunction();
		});

		return Parameters;
	}

	static FString GetParameterDisplayName(const FProperty* Parameter)
	{
		return Parameter != nullptr ? Parameter->GetMetaData(TEXT("DisplayName")) : FString();
	}

	static bool HasAllFunctionFlags(const UFunction* Function, EFunctionFlags RequiredFlags)
	{
		return Function != nullptr && Function->HasAllFunctionFlags(RequiredFlags);
	}

	static bool IsFunctionClassOneOf(const UFunction* Function, const UClass* ExpectedClass, const UClass* ExpectedJitClass = nullptr)
	{
		if (Function == nullptr)
		{
			return false;
		}

		const UClass* ActualClass = Function->GetClass();
		return ActualClass == ExpectedClass || (ExpectedJitClass != nullptr && ActualClass == ExpectedJitClass);
	}

	static bool ReadIntProperty(FAutomationTestBase& Test, UObject* Object, FName PropertyName, int32& OutValue)
	{
		return ReadPropertyValue<FIntProperty>(Test, Object, PropertyName, OutValue);
	}

	static bool AddIntArrayValue(FAutomationTestBase& Test, const FArrayProperty& ArrayProperty, void* ArrayAddress, int32 Value)
	{
		FNoDiscardAsserter LocalAssert(Test);
		FIntProperty* InnerProperty = CastField<FIntProperty>(ArrayProperty.Inner);
		if (!LocalAssert.IsNotNull(InnerProperty, TEXT("TArray<int> inner property should be FIntProperty")))
		{
			return false;
		}

		FScriptArrayHelper Helper(&ArrayProperty, ArrayAddress);
		const int32 Index = Helper.AddValue();
		void* ItemAddress = Helper.GetRawPtr(Index);
		if (!LocalAssert.IsNotNull(ItemAddress, TEXT("TArray<int> added item should expose writable memory")))
		{
			return false;
		}

		InnerProperty->SetPropertyValue(ItemAddress, Value);
		return true;
	}

	static bool AddNameIntMapValue(FAutomationTestBase& Test, const FMapProperty& MapProperty, void* MapAddress, FName Key, int32 Value)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const FNameProperty* KeyProperty = CastField<const FNameProperty>(MapProperty.KeyProp);
		const FIntProperty* ValueProperty = CastField<const FIntProperty>(MapProperty.ValueProp);
		if (!LocalAssert.IsNotNull(KeyProperty, TEXT("TMap<FName,int> key property should be FNameProperty"))
			|| !LocalAssert.IsNotNull(ValueProperty, TEXT("TMap<FName,int> value property should be FIntProperty")))
		{
			return false;
		}

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		Helper.AddPair(&Key, &Value);
		return true;
	}

	static bool AddIntSetValue(FAutomationTestBase& Test, const FSetProperty& SetProperty, void* SetAddress, int32 Value)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const FIntProperty* ElementProperty = CastField<const FIntProperty>(SetProperty.ElementProp);
		if (!LocalAssert.IsNotNull(ElementProperty, TEXT("TSet<int> element property should be FIntProperty")))
		{
			return false;
		}

		FScriptSetHelper Helper(&SetProperty, SetAddress);
		const int32 Index = Helper.AddDefaultValue_Invalid_NeedsRehash();
		void* ElementAddress = Helper.GetElementPtr(Index);
		if (!LocalAssert.IsNotNull(ElementAddress, TEXT("TSet<int> added element should expose writable memory")))
		{
			return false;
		}

		ElementProperty->SetPropertyValue(ElementAddress, Value);
		Helper.Rehash();
		return true;
	}

	static bool FindNameIntMapValue(FAutomationTestBase& Test, const FMapProperty& MapProperty, const void* MapAddress, FName Key, int32& OutValue)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const FNameProperty* KeyProperty = CastField<const FNameProperty>(MapProperty.KeyProp);
		const FIntProperty* ValueProperty = CastField<const FIntProperty>(MapProperty.ValueProp);
		if (!LocalAssert.IsNotNull(KeyProperty, TEXT("TMap<FName,int> key property should be FNameProperty"))
			|| !LocalAssert.IsNotNull(ValueProperty, TEXT("TMap<FName,int> value property should be FIntProperty")))
		{
			return false;
		}

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex(); ++SparseIndex)
		{
			if (!Helper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			const FName ActualKey = KeyProperty->GetPropertyValue(Helper.GetKeyPtr(SparseIndex));
			if (ActualKey == Key)
			{
				OutValue = ValueProperty->GetPropertyValue(Helper.GetValuePtr(SparseIndex));
				return true;
			}
		}

		return LocalAssert.IsTrue(false, *FString::Printf(TEXT("TMap<FName,int> should contain key '%s'"), *Key.ToString()));
	}

	static bool SetContainsIntValue(FAutomationTestBase& Test, const FSetProperty& SetProperty, const void* SetAddress, int32 ExpectedValue)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const FIntProperty* ElementProperty = CastField<const FIntProperty>(SetProperty.ElementProp);
		if (!LocalAssert.IsNotNull(ElementProperty, TEXT("TSet<int> element property should be FIntProperty")))
		{
			return false;
		}

		FScriptSetHelper Helper(&SetProperty, SetAddress);
		for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex(); ++SparseIndex)
		{
			if (!Helper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			if (ElementProperty->GetPropertyValue(Helper.GetElementPtr(SparseIndex)) == ExpectedValue)
			{
				return true;
			}
		}

		return LocalAssert.IsTrue(false, *FString::Printf(TEXT("TSet<int> should contain value %d"), ExpectedValue));
	}

	static bool WritePayloadStructValue(
		FAutomationTestBase& Test,
		const FStructProperty& StructProperty,
		void* StructAddress,
		int32 Count,
		const FString& Label)
	{
		FNoDiscardAsserter LocalAssert(Test);
		FIntProperty* CountProperty = FindFProperty<FIntProperty>(StructProperty.Struct, TEXT("Count"));
		FStrProperty* LabelProperty = FindFProperty<FStrProperty>(StructProperty.Struct, TEXT("Label"));
		if (!LocalAssert.IsNotNull(CountProperty, TEXT("payload struct should expose Count"))
			|| !LocalAssert.IsNotNull(LabelProperty, TEXT("payload struct should expose Label")))
		{
			return false;
		}

		CountProperty->SetPropertyValue_InContainer(StructAddress, Count);
		LabelProperty->SetPropertyValue_InContainer(StructAddress, Label);
		return true;
	}

	static bool VerifyPayloadStructValue(
		FAutomationTestBase& Test,
		const FStructProperty& StructProperty,
		const void* StructAddress,
		int32 ExpectedCount,
		const FString& ExpectedLabel,
		const TCHAR* MessagePrefix)
	{
		FNoDiscardAsserter LocalAssert(Test);
		FIntProperty* CountProperty = FindFProperty<FIntProperty>(StructProperty.Struct, TEXT("Count"));
		FStrProperty* LabelProperty = FindFProperty<FStrProperty>(StructProperty.Struct, TEXT("Label"));
		if (!LocalAssert.IsNotNull(CountProperty, TEXT("payload struct should expose Count"))
			|| !LocalAssert.IsNotNull(LabelProperty, TEXT("payload struct should expose Label")))
		{
			return false;
		}

		const int32 ActualCount = CountProperty->GetPropertyValue_InContainer(StructAddress);
		const FString ActualLabel = LabelProperty->GetPropertyValue_InContainer(StructAddress);
		const FString CountMessage = FString::Printf(TEXT("%s Count should match"), MessagePrefix);
		const FString LabelMessage = FString::Printf(TEXT("%s Label should match"), MessagePrefix);
		return LocalAssert.AreEqual(ExpectedCount, ActualCount, *CountMessage)
			&& LocalAssert.AreEqual(ExpectedLabel, ActualLabel, *LabelMessage);
	}

	static bool CompileSummaryHasErrorContaining(const FAngelscriptCompileTraceSummary& Summary, const TCHAR* ExpectedFragment)
	{
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
		{
			if (Diagnostic.bIsError && Diagnostic.Message.Contains(ExpectedFragment))
			{
				return true;
			}
		}

		return false;
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

	TEST_METHOD(UFunctionSpecifiersAndMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_SpecifiersAndMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionSpecifiersAndMetadata.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionSpecifiersActor : AActor
			{
				UPROPERTY()
				int StoredValue = 0;

				UFUNCTION(BlueprintCallable, Category="Coverage|Functions", CallInEditor, meta=(DisplayName="Visible Action", Keywords="coverage keyword action", ToolTip="Function tooltip text", ShortToolTip="Short function tooltip", CompactNodeTitle="ACT"))
				void VisibleAction(int Value)
				{
					StoredValue = Value;
				}

				UFUNCTION(BlueprintPure, Category="Coverage|Functions", meta=(DisplayName="Read Stored Value"))
				int ReadStoredValue() const
				{
					return StoredValue;
				}

				UFUNCTION(Exec, Category="Coverage|Console")
				void CoverageExecCommand()
				{
					StoredValue = 77;
				}
			}
			)AS"),
			TEXT("ACoverageUFunctionSpecifiersActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UFUNCTION specifier actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* VisibleAction = FindFunctionForTest(ScriptClass, TEXT("VisibleAction"));
		ASSERT_THAT(IsNotNull(VisibleAction, TEXT("VisibleAction should be generated")));
		if (VisibleAction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(VisibleAction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("BlueprintCallable should set FUNC_BlueprintCallable")));
		ASSERT_THAT(IsFalse(VisibleAction->HasAnyFunctionFlags(FUNC_BlueprintPure),
			TEXT("BlueprintCallable action should not be pure")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Functions")), VisibleAction->GetMetaData(TEXT("Category")),
			TEXT("UFUNCTION Category should be preserved")));
		ASSERT_THAT(IsTrue(VisibleAction->HasMetaData(TEXT("CallInEditor")),
			TEXT("CallInEditor should be preserved as function metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Visible Action")), VisibleAction->GetMetaData(TEXT("DisplayName")),
			TEXT("meta DisplayName should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("coverage keyword action")), VisibleAction->GetMetaData(TEXT("Keywords")),
			TEXT("meta Keywords should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("Function tooltip text")), VisibleAction->GetMetaData(TEXT("ToolTip")),
			TEXT("meta ToolTip should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("Short function tooltip")), VisibleAction->GetMetaData(TEXT("ShortToolTip")),
			TEXT("meta ShortToolTip should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("ACT")), VisibleAction->GetMetaData(TEXT("CompactNodeTitle")),
			TEXT("meta CompactNodeTitle should be preserved")));

		UFunction* ReadStoredValue = FindFunctionForTest(ScriptClass, TEXT("ReadStoredValue"));
		ASSERT_THAT(IsNotNull(ReadStoredValue, TEXT("ReadStoredValue should be generated")));
		if (ReadStoredValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ReadStoredValue->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("BlueprintPure should also be BlueprintCallable for reflection")));
		ASSERT_THAT(IsTrue(ReadStoredValue->HasAnyFunctionFlags(FUNC_BlueprintPure),
			TEXT("BlueprintPure should set FUNC_BlueprintPure")));
		ASSERT_THAT(IsTrue(ReadStoredValue->HasAnyFunctionFlags(FUNC_Const),
			TEXT("const method should set FUNC_Const")));
		ASSERT_THAT(AreEqual(FString(TEXT("Read Stored Value")), ReadStoredValue->GetMetaData(TEXT("DisplayName")),
			TEXT("BlueprintPure DisplayName metadata should be preserved")));

		UFunction* CoverageExecCommand = FindFunctionForTest(ScriptClass, TEXT("CoverageExecCommand"));
		ASSERT_THAT(IsNotNull(CoverageExecCommand, TEXT("CoverageExecCommand should be generated")));
		if (CoverageExecCommand == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(CoverageExecCommand->HasAnyFunctionFlags(FUNC_Exec),
			TEXT("Exec should set FUNC_Exec")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Console")), CoverageExecCommand->GetMetaData(TEXT("Category")),
			TEXT("Exec function Category should be preserved")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UFUNCTION specifier actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		FFunctionInvoker VisibleActionInvoker(*TestRunner, Actor, TEXT("VisibleAction"));
		ASSERT_THAT(IsTrue(VisibleActionInvoker.IsValid(), TEXT("VisibleAction should be invokable through reflection")));
		if (!VisibleActionInvoker.IsValid())
		{
			return;
		}
		VisibleActionInvoker.AddParam<int32>(42);
		ASSERT_THAT(IsTrue(VisibleActionInvoker.Call(), TEXT("VisibleAction reflected invocation should succeed")));

		FFunctionInvoker ReadStoredValueInvoker(*TestRunner, Actor, TEXT("ReadStoredValue"));
		ASSERT_THAT(IsTrue(ReadStoredValueInvoker.IsValid(), TEXT("ReadStoredValue should be invokable through reflection")));
		if (!ReadStoredValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, ReadStoredValueInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("BlueprintCallable reflected invocation should update state visible to the pure getter")));

		FFunctionInvoker CoverageExecInvoker(*TestRunner, Actor, TEXT("CoverageExecCommand"));
		ASSERT_THAT(IsTrue(CoverageExecInvoker.IsValid(), TEXT("CoverageExecCommand should be invokable through reflection")));
		if (!CoverageExecInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(CoverageExecInvoker.Call(), TEXT("Exec reflected invocation should succeed")));
		ASSERT_THAT(AreEqual(77, ReadStoredValueInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Exec reflected invocation should update script actor state")));
	}

	TEST_METHOD(UFunctionParameterMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_ParameterMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionParameterMetadata.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionParameterActor : AActor
			{
				UFUNCTION(BlueprintCallable, Category="Coverage|Parameters", meta=(
					AdvancedDisplay="OptionalValue,OptionalLabel",
					DefaultToSelf="Target",
					HidePin="Target",
					AutoCreateRefTerm="OptionalLabel"))
				void ConfigureAdvanced(UObject Target, int RequiredValue, int OptionalValue, const FString&in OptionalLabel)
				{
				}
			}
			)AS"),
			TEXT("ACoverageUFunctionParameterActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UFUNCTION parameter metadata actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* ConfigureAdvanced = FindFunctionForTest(ScriptClass, TEXT("ConfigureAdvanced"));
		ASSERT_THAT(IsNotNull(ConfigureAdvanced, TEXT("ConfigureAdvanced should be generated")));
		if (ConfigureAdvanced == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("OptionalValue,OptionalLabel")), ConfigureAdvanced->GetMetaData(TEXT("AdvancedDisplay")),
			TEXT("AdvancedDisplay metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("Target")), ConfigureAdvanced->GetMetaData(TEXT("DefaultToSelf")),
			TEXT("DefaultToSelf metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("Target")), ConfigureAdvanced->GetMetaData(TEXT("HidePin")),
			TEXT("HidePin metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("OptionalLabel")), ConfigureAdvanced->GetMetaData(TEXT("AutoCreateRefTerm")),
			TEXT("AutoCreateRefTerm metadata should be preserved")));

		FProperty* TargetParam = FindParameterForTest(ConfigureAdvanced, TEXT("Target"));
		ASSERT_THAT(IsNotNull(TargetParam, TEXT("Target parameter should be reflected")));
		if (TargetParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(TargetParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("Target should not be marked AdvancedDisplay")));

		FProperty* RequiredValueParam = FindParameterForTest(ConfigureAdvanced, TEXT("RequiredValue"));
		ASSERT_THAT(IsNotNull(RequiredValueParam, TEXT("RequiredValue parameter should be reflected")));
		if (RequiredValueParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(RequiredValueParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("RequiredValue should not be marked AdvancedDisplay")));

		FProperty* OptionalValueParam = FindParameterForTest(ConfigureAdvanced, TEXT("OptionalValue"));
		ASSERT_THAT(IsNotNull(OptionalValueParam, TEXT("OptionalValue parameter should be reflected")));
		if (OptionalValueParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(OptionalValueParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("OptionalValue should be marked AdvancedDisplay")));

		FProperty* OptionalLabelParam = FindParameterForTest(ConfigureAdvanced, TEXT("OptionalLabel"));
		ASSERT_THAT(IsNotNull(OptionalLabelParam, TEXT("OptionalLabel parameter should be reflected")));
		if (OptionalLabelParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(OptionalLabelParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("OptionalLabel should be marked AdvancedDisplay")));
	}

	TEST_METHOD(UParamDisplayNameAndRefMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_UParamMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionUParamActor : AActor
			{
				UPROPERTY()
				int LastResult = 0;

				UPROPERTY()
				FString LastOutput;

				UFUNCTION(BlueprintCallable, Category="Coverage|UParam")
				int ProcessWithDisplayNames(
					UPARAM(DisplayName="Input Number") int Input,
					UPARAM(DisplayName="Scale Value") int Scale,
					UPARAM(DisplayName="Adjusted Value", ref) int&out Adjusted)
				{
					Adjusted = Input * Scale;
					LastResult = Adjusted + 2;
					return LastResult;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|UParam")
				void SplitWithRefs(
					UPARAM(DisplayName="Source Value") int Source,
					UPARAM(ref) int&out Left,
					UPARAM(ref) int&out Right)
				{
					Left = Source / 2;
					Right = Source - Left;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|UParam")
				void TransformString(
					UPARAM(DisplayName="Input Text") const FString&in Input,
					UPARAM(DisplayName="Output Text", ref) FString&out Output)
				{
					Output = Input + ":processed";
					LastOutput = Output;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionUParamMatrix.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionUParamActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UPARAM UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* ProcessWithDisplayNames = FindFunctionForTest(ScriptClass, TEXT("ProcessWithDisplayNames"));
		UFunction* SplitWithRefs = FindFunctionForTest(ScriptClass, TEXT("SplitWithRefs"));
		UFunction* TransformString = FindFunctionForTest(ScriptClass, TEXT("TransformString"));
		ASSERT_THAT(IsNotNull(ProcessWithDisplayNames, TEXT("ProcessWithDisplayNames should be generated")));
		ASSERT_THAT(IsNotNull(SplitWithRefs, TEXT("SplitWithRefs should be generated")));
		ASSERT_THAT(IsNotNull(TransformString, TEXT("TransformString should be generated")));
		if (ProcessWithDisplayNames == nullptr || SplitWithRefs == nullptr || TransformString == nullptr)
		{
			return;
		}

		const TArray<FProperty*> ProcessParams = GetOrderedParameters(ProcessWithDisplayNames);
		ASSERT_THAT(AreEqual(3, ProcessParams.Num(), TEXT("ProcessWithDisplayNames should expose three parameters in declaration order")));
		ASSERT_THAT(AreEqual(FString(TEXT("Input Number")), GetParameterDisplayName(ProcessParams.IsValidIndex(0) ? ProcessParams[0] : nullptr),
			TEXT("UPARAM DisplayName should round-trip on first scalar parameter")));
		ASSERT_THAT(AreEqual(FString(TEXT("Scale Value")), GetParameterDisplayName(ProcessParams.IsValidIndex(1) ? ProcessParams[1] : nullptr),
			TEXT("UPARAM DisplayName should round-trip on second scalar parameter")));
		ASSERT_THAT(AreEqual(FString(TEXT("Adjusted Value")), GetParameterDisplayName(ProcessParams.IsValidIndex(2) ? ProcessParams[2] : nullptr),
			TEXT("UPARAM DisplayName should round-trip on out parameter")));
		ASSERT_THAT(IsTrue(ProcessParams.IsValidIndex(2) && ProcessParams[2]->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("UPARAM(ref) int &out parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(ProcessParams.IsValidIndex(2) && ProcessParams[2]->HasAnyPropertyFlags(CPF_ReferenceParm),
			TEXT("UPARAM(ref) int &out parameter should remain an out-only parameter")));

		FIntProperty* ProcessReturn = CastField<FIntProperty>(ProcessWithDisplayNames->GetReturnProperty());
		ASSERT_THAT(IsNotNull(ProcessReturn, TEXT("ProcessWithDisplayNames should reflect int return")));
		ASSERT_THAT(IsTrue(ProcessReturn != nullptr && ProcessReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("ProcessWithDisplayNames return should carry CPF_ReturnParm")));

		const TArray<FProperty*> SplitParams = GetOrderedParameters(SplitWithRefs);
		ASSERT_THAT(AreEqual(3, SplitParams.Num(), TEXT("SplitWithRefs should expose three parameters in declaration order")));
		ASSERT_THAT(AreEqual(FString(TEXT("Source Value")), GetParameterDisplayName(SplitParams.IsValidIndex(0) ? SplitParams[0] : nullptr),
			TEXT("UPARAM DisplayName should round-trip on SplitWithRefs input")));
		ASSERT_THAT(IsTrue(SplitParams.IsValidIndex(1) && SplitParams[1]->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("UPARAM(ref) Left parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsTrue(SplitParams.IsValidIndex(2) && SplitParams[2]->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("UPARAM(ref) Right parameter should carry CPF_OutParm")));

		const TArray<FProperty*> StringParams = GetOrderedParameters(TransformString);
		ASSERT_THAT(AreEqual(2, StringParams.Num(), TEXT("TransformString should expose two parameters in declaration order")));
		ASSERT_THAT(AreEqual(FString(TEXT("Input Text")), GetParameterDisplayName(StringParams.IsValidIndex(0) ? StringParams[0] : nullptr),
			TEXT("UPARAM DisplayName should round-trip on const-ref FString input")));
		ASSERT_THAT(AreEqual(FString(TEXT("Output Text")), GetParameterDisplayName(StringParams.IsValidIndex(1) ? StringParams[1] : nullptr),
			TEXT("UPARAM DisplayName should round-trip on FString out parameter")));
		ASSERT_THAT(IsTrue(StringParams.IsValidIndex(0) && StringParams[0]->HasAllPropertyFlags(CPF_ConstParm | CPF_OutParm | CPF_ReferenceParm),
			TEXT("const FString&in UPARAM should carry const/out/reference flags")));
		ASSERT_THAT(IsTrue(StringParams.IsValidIndex(1) && StringParams[1]->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("UPARAM(ref) FString &out parameter should carry CPF_OutParm")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UPARAM UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker ProcessInvoker(*TestRunner, Actor, TEXT("ProcessWithDisplayNames"));
		ASSERT_THAT(IsTrue(ProcessInvoker.IsValid(), TEXT("ProcessWithDisplayNames should be invokable")));
		if (!ProcessInvoker.IsValid())
		{
			return;
		}
		ProcessInvoker.AddParam<int32>(7);
		ProcessInvoker.AddParam<int32>(5);
		FProperty* AdjustedSlotProperty = nullptr;
		void* AdjustedSlot = nullptr;
		ASSERT_THAT(IsTrue(ProcessInvoker.AddParamSlot(AdjustedSlotProperty, AdjustedSlot),
			TEXT("ProcessWithDisplayNames should expose adjusted out slot")));
		FIntProperty* AdjustedIntProperty = CastField<FIntProperty>(AdjustedSlotProperty);
		ASSERT_THAT(IsNotNull(AdjustedIntProperty, TEXT("Adjusted out slot should be FIntProperty")));
		if (AdjustedSlot == nullptr || AdjustedIntProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(37, ProcessInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("ProcessWithDisplayNames should return value based on UPARAM out calculation")));
		ASSERT_THAT(AreEqual(35, AdjustedIntProperty->GetPropertyValue(AdjustedSlot),
			TEXT("ProcessWithDisplayNames should write adjusted out value")));

		FFunctionInvoker SplitInvoker(*TestRunner, Actor, TEXT("SplitWithRefs"));
		ASSERT_THAT(IsTrue(SplitInvoker.IsValid(), TEXT("SplitWithRefs should be invokable")));
		if (!SplitInvoker.IsValid())
		{
			return;
		}
		SplitInvoker.AddParam<int32>(41);
		FProperty* LeftSlotProperty = nullptr;
		void* LeftSlot = nullptr;
		FProperty* RightSlotProperty = nullptr;
		void* RightSlot = nullptr;
		ASSERT_THAT(IsTrue(SplitInvoker.AddParamSlot(LeftSlotProperty, LeftSlot), TEXT("SplitWithRefs should expose Left out slot")));
		ASSERT_THAT(IsTrue(SplitInvoker.AddParamSlot(RightSlotProperty, RightSlot), TEXT("SplitWithRefs should expose Right out slot")));
		FIntProperty* LeftIntProperty = CastField<FIntProperty>(LeftSlotProperty);
		FIntProperty* RightIntProperty = CastField<FIntProperty>(RightSlotProperty);
		ASSERT_THAT(IsNotNull(LeftIntProperty, TEXT("Left out slot should be FIntProperty")));
		ASSERT_THAT(IsNotNull(RightIntProperty, TEXT("Right out slot should be FIntProperty")));
		if (LeftSlot == nullptr || RightSlot == nullptr || LeftIntProperty == nullptr || RightIntProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(SplitInvoker.Call(), TEXT("SplitWithRefs should write both UPARAM ref outputs")));
		ASSERT_THAT(AreEqual(20, LeftIntProperty->GetPropertyValue(LeftSlot), TEXT("SplitWithRefs should write Left")));
		ASSERT_THAT(AreEqual(21, RightIntProperty->GetPropertyValue(RightSlot), TEXT("SplitWithRefs should write Right")));

		FFunctionInvoker StringInvoker(*TestRunner, Actor, TEXT("TransformString"));
		ASSERT_THAT(IsTrue(StringInvoker.IsValid(), TEXT("TransformString should be invokable")));
		if (!StringInvoker.IsValid())
		{
			return;
		}
		StringInvoker.AddParam<FString>(FString(TEXT("Input")));
		FProperty* OutputSlotProperty = nullptr;
		void* OutputSlot = nullptr;
		ASSERT_THAT(IsTrue(StringInvoker.AddParamSlot(OutputSlotProperty, OutputSlot),
			TEXT("TransformString should expose FString out slot")));
		FStrProperty* OutputStringProperty = CastField<FStrProperty>(OutputSlotProperty);
		ASSERT_THAT(IsNotNull(OutputStringProperty, TEXT("TransformString out slot should be FStrProperty")));
		if (OutputSlot == nullptr || OutputStringProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(StringInvoker.Call(), TEXT("TransformString should execute through UPARAM ref output")));
		ASSERT_THAT(AreEqual(FString(TEXT("Input:processed")), OutputStringProperty->GetPropertyValue(OutputSlot),
			TEXT("TransformString should write FString out parameter")));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastOutput"), FString(TEXT("Input:processed")),
			TEXT("TransformString should update reflected state"))));
	}

	TEST_METHOD(UFunctionSpecifierFlagEdges)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_SpecifierFlagEdges"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionFlagEdgeActor : AActor
			{
				UPROPERTY()
				int StoredValue = 0;

				UFUNCTION(NotBlueprintCallable)
				void HiddenAction(int Value)
				{
					StoredValue = Value;
				}

				UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, BlueprintProtected, meta=(DeprecatedFunction, DeprecationMessage="Use ReplacementAction"))
				void AuthorityProtectedAction(int Value)
				{
					StoredValue = Value + 10;
				}

				UFUNCTION(BlueprintCallable)
				int ReadStoredValue() const
				{
					return StoredValue;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionSpecifierFlagEdges.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionFlagEdgeActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UFUNCTION flag-edge actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* HiddenAction = FindFunctionForTest(ScriptClass, TEXT("HiddenAction"));
		UFunction* AuthorityProtectedAction = FindFunctionForTest(ScriptClass, TEXT("AuthorityProtectedAction"));
		UFunction* ReadStoredValue = FindFunctionForTest(ScriptClass, TEXT("ReadStoredValue"));
		ASSERT_THAT(IsNotNull(HiddenAction, TEXT("NotBlueprintCallable UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(AuthorityProtectedAction, TEXT("authority/protected UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReadStoredValue, TEXT("flag-edge getter should be generated")));
		if (HiddenAction == nullptr || AuthorityProtectedAction == nullptr || ReadStoredValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(HiddenAction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("NotBlueprintCallable should suppress default FUNC_BlueprintCallable")));
		ASSERT_THAT(IsFalse(HiddenAction->HasAnyFunctionFlags(FUNC_BlueprintPure | FUNC_BlueprintEvent),
			TEXT("NotBlueprintCallable should not accidentally add pure or event flags")));
		ASSERT_THAT(IsTrue(AuthorityProtectedAction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("BlueprintCallable should still be present on authority/protected functions")));
		ASSERT_THAT(IsTrue(AuthorityProtectedAction->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly),
			TEXT("BlueprintAuthorityOnly should set FUNC_BlueprintAuthorityOnly")));
		ASSERT_THAT(IsTrue(AuthorityProtectedAction->HasMetaData(TEXT("BlueprintProtected")),
			TEXT("BlueprintProtected should round-trip as UFunction metadata")));
		ASSERT_THAT(IsTrue(AuthorityProtectedAction->HasMetaData(TEXT("DeprecatedFunction")),
			TEXT("DeprecatedFunction metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("Use ReplacementAction")), AuthorityProtectedAction->GetMetaData(TEXT("DeprecationMessage")),
			TEXT("DeprecationMessage metadata should be preserved")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UFUNCTION flag-edge actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker HiddenInvoker(*TestRunner, Actor, TEXT("HiddenAction"));
		ASSERT_THAT(IsTrue(HiddenInvoker.IsValid(), TEXT("NotBlueprintCallable generated UFUNCTION should remain reflectively invokable")));
		if (!HiddenInvoker.IsValid())
		{
			return;
		}
		HiddenInvoker.AddParam<int32>(31);
		ASSERT_THAT(IsTrue(HiddenInvoker.Call(), TEXT("NotBlueprintCallable generated UFUNCTION should execute")));

		FFunctionInvoker GetterAfterHidden(*TestRunner, Actor, TEXT("ReadStoredValue"));
		ASSERT_THAT(IsTrue(GetterAfterHidden.IsValid(), TEXT("flag-edge getter should be invokable after hidden call")));
		if (!GetterAfterHidden.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(31, GetterAfterHidden.CallAndReturn<int32>(INDEX_NONE),
			TEXT("NotBlueprintCallable invocation should update actor state")));

		FFunctionInvoker AuthorityInvoker(*TestRunner, Actor, TEXT("AuthorityProtectedAction"));
		ASSERT_THAT(IsTrue(AuthorityInvoker.IsValid(), TEXT("authority/protected UFUNCTION should be reflectively invokable")));
		if (!AuthorityInvoker.IsValid())
		{
			return;
		}
		AuthorityInvoker.AddParam<int32>(32);
		ASSERT_THAT(IsTrue(AuthorityInvoker.Call(), TEXT("authority/protected UFUNCTION should execute")));

		FFunctionInvoker GetterAfterAuthority(*TestRunner, Actor, TEXT("ReadStoredValue"));
		ASSERT_THAT(IsTrue(GetterAfterAuthority.IsValid(), TEXT("flag-edge getter should be invokable after authority call")));
		if (!GetterAfterAuthority.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, GetterAfterAuthority.CallAndReturn<int32>(INDEX_NONE),
			TEXT("authority/protected invocation should update actor state")));
	}

	TEST_METHOD(EditorExecAndAuthoritySpecifierCombinations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_EditorExecAuthority"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionEditorExecActor : AActor
			{
				UPROPERTY()
				int StoredValue = 0;

				UFUNCTION(Exec, Category="Coverage|Console")
				void PlainExecCommand(int Value)
				{
					StoredValue = Value + 1;
				}

				UFUNCTION(BlueprintCallable, Exec, CallInEditor, Category="Coverage|Console")
				void CallableEditorExecCommand(int Value)
				{
					StoredValue = Value + 2;
				}

				UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, CallInEditor, Category="Coverage|Authority")
				void AuthorityEditorAction(int Value)
				{
					StoredValue = Value + 3;
				}

				UFUNCTION(NotBlueprintCallable, Exec, CallInEditor, Category="Coverage|Console")
				void HiddenEditorExecCommand(int Value)
				{
					StoredValue = Value + 4;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Console")
				int ReadStoredValue() const
				{
					return StoredValue;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionEditorExecAuthority.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionEditorExecActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("editor/exec/authority UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* PlainExecCommand = FindFunctionForTest(ScriptClass, TEXT("PlainExecCommand"));
		UFunction* CallableEditorExecCommand = FindFunctionForTest(ScriptClass, TEXT("CallableEditorExecCommand"));
		UFunction* AuthorityEditorAction = FindFunctionForTest(ScriptClass, TEXT("AuthorityEditorAction"));
		UFunction* HiddenEditorExecCommand = FindFunctionForTest(ScriptClass, TEXT("HiddenEditorExecCommand"));
		UFunction* ReadStoredValue = FindFunctionForTest(ScriptClass, TEXT("ReadStoredValue"));
		ASSERT_THAT(IsNotNull(PlainExecCommand, TEXT("plain Exec UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(CallableEditorExecCommand, TEXT("BlueprintCallable Exec CallInEditor UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(AuthorityEditorAction, TEXT("authority CallInEditor UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(HiddenEditorExecCommand, TEXT("hidden Exec CallInEditor UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReadStoredValue, TEXT("editor/exec readback UFUNCTION should be generated")));
		if (PlainExecCommand == nullptr || CallableEditorExecCommand == nullptr || AuthorityEditorAction == nullptr
			|| HiddenEditorExecCommand == nullptr || ReadStoredValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(PlainExecCommand->HasAnyFunctionFlags(FUNC_Exec),
			TEXT("Exec should set FUNC_Exec on a non-callable UFUNCTION")));
		ASSERT_THAT(IsFalse(PlainExecCommand->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("plain Exec should not imply BlueprintCallable")));
		ASSERT_THAT(IsFalse(PlainExecCommand->HasMetaData(TEXT("CallInEditor")),
			TEXT("plain Exec should not gain CallInEditor metadata")));

		ASSERT_THAT(IsTrue(HasAllFunctionFlags(CallableEditorExecCommand, FUNC_Exec | FUNC_BlueprintCallable),
			TEXT("BlueprintCallable Exec CallInEditor should carry both exec and callable flags")));
		ASSERT_THAT(IsTrue(CallableEditorExecCommand->HasMetaData(TEXT("CallInEditor")),
			TEXT("CallInEditor specifier should be preserved on callable exec functions")));

		ASSERT_THAT(IsTrue(HasAllFunctionFlags(AuthorityEditorAction, FUNC_BlueprintCallable | FUNC_BlueprintAuthorityOnly),
			TEXT("BlueprintAuthorityOnly CallInEditor action should carry callable and authority flags")));
		ASSERT_THAT(IsTrue(AuthorityEditorAction->HasMetaData(TEXT("CallInEditor")),
			TEXT("CallInEditor metadata should coexist with BlueprintAuthorityOnly")));
		ASSERT_THAT(IsFalse(AuthorityEditorAction->HasAnyFunctionFlags(FUNC_Exec),
			TEXT("authority CallInEditor action should not accidentally become Exec")));

		ASSERT_THAT(IsTrue(HiddenEditorExecCommand->HasAnyFunctionFlags(FUNC_Exec),
			TEXT("NotBlueprintCallable Exec should still carry FUNC_Exec")));
		ASSERT_THAT(IsFalse(HiddenEditorExecCommand->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("NotBlueprintCallable should suppress callable even with Exec and CallInEditor")));
		ASSERT_THAT(IsTrue(HiddenEditorExecCommand->HasMetaData(TEXT("CallInEditor")),
			TEXT("CallInEditor metadata should survive NotBlueprintCallable")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("editor/exec/authority UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		struct FInvocationCase
		{
			const TCHAR* FunctionName;
			int32 InputValue;
			int32 ExpectedStoredValue;
		};

		const FInvocationCase InvocationCases[] = {
			{ TEXT("PlainExecCommand"), 10, 11 },
			{ TEXT("CallableEditorExecCommand"), 20, 22 },
			{ TEXT("AuthorityEditorAction"), 30, 33 },
			{ TEXT("HiddenEditorExecCommand"), 38, 42 },
		};

		for (const FInvocationCase& InvocationCase : InvocationCases)
		{
			FFunctionInvoker ActionInvoker(*TestRunner, Actor, InvocationCase.FunctionName);
			const FString ValidMessage = FString::Printf(TEXT("%s should be reflectively invokable"), InvocationCase.FunctionName);
			ASSERT_THAT(IsTrue(ActionInvoker.IsValid(), *ValidMessage));
			if (!ActionInvoker.IsValid())
			{
				return;
			}
			ActionInvoker.AddParam<int32>(InvocationCase.InputValue);
			const FString CallMessage = FString::Printf(TEXT("%s should execute through reflection"), InvocationCase.FunctionName);
			ASSERT_THAT(IsTrue(ActionInvoker.Call(), *CallMessage));

			FFunctionInvoker ReadInvoker(*TestRunner, Actor, TEXT("ReadStoredValue"));
			ASSERT_THAT(IsTrue(ReadInvoker.IsValid(), TEXT("ReadStoredValue should be invokable after editor/exec action")));
			if (!ReadInvoker.IsValid())
			{
				return;
			}
			const FString ResultMessage = FString::Printf(TEXT("%s should update StoredValue through its script body"), InvocationCase.FunctionName);
			ASSERT_THAT(AreEqual(InvocationCase.ExpectedStoredValue, ReadInvoker.CallAndReturn<int32>(INDEX_NONE),
				*ResultMessage));
		}
	}

	TEST_METHOD(BlueprintCallableSuppressionAndSpecifierOrderMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_CallableSpecifierOrder"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionCallableOrderActor : AActor
			{
				UPROPERTY()
				int StoredValue = 0;

				UFUNCTION(BlueprintCallable, NotBlueprintCallable, Category="Coverage|CallableOrder")
				void CallableThenHidden(int Value)
				{
					StoredValue = Value + 1;
				}

				UFUNCTION(NotBlueprintCallable, BlueprintCallable, Category="Coverage|CallableOrder")
				void HiddenThenCallable(int Value)
				{
					StoredValue = Value + 2;
				}

				UFUNCTION(BlueprintPure, NotBlueprintCallable, Category="Coverage|CallableOrder")
				int PureThenHidden(int Value) const
				{
					return StoredValue + Value + 3;
				}

				UFUNCTION(NotBlueprintCallable, BlueprintPure, Category="Coverage|CallableOrder")
				int HiddenThenPure(int Value) const
				{
					return StoredValue + Value + 4;
				}

				UFUNCTION(NotBlueprintCallable, Exec, Category="Coverage|CallableOrder")
				void HiddenExecCommand(int Value)
				{
					StoredValue = Value + 5;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|CallableOrder")
				int ReadStoredValue() const
				{
					return StoredValue;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionCallableSpecifierOrder.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionCallableOrderActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("callable specifier order actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* CallableThenHidden = FindFunctionForTest(ScriptClass, TEXT("CallableThenHidden"));
		UFunction* HiddenThenCallable = FindFunctionForTest(ScriptClass, TEXT("HiddenThenCallable"));
		UFunction* PureThenHidden = FindFunctionForTest(ScriptClass, TEXT("PureThenHidden"));
		UFunction* HiddenThenPure = FindFunctionForTest(ScriptClass, TEXT("HiddenThenPure"));
		UFunction* HiddenExecCommand = FindFunctionForTest(ScriptClass, TEXT("HiddenExecCommand"));
		UFunction* ReadStoredValue = FindFunctionForTest(ScriptClass, TEXT("ReadStoredValue"));
		ASSERT_THAT(IsNotNull(CallableThenHidden, TEXT("BlueprintCallable then NotBlueprintCallable function should be generated")));
		ASSERT_THAT(IsNotNull(HiddenThenCallable, TEXT("NotBlueprintCallable then BlueprintCallable function should be generated")));
		ASSERT_THAT(IsNotNull(PureThenHidden, TEXT("BlueprintPure then NotBlueprintCallable function should be generated")));
		ASSERT_THAT(IsNotNull(HiddenThenPure, TEXT("NotBlueprintCallable then BlueprintPure function should be generated")));
		ASSERT_THAT(IsNotNull(HiddenExecCommand, TEXT("NotBlueprintCallable Exec function should be generated")));
		ASSERT_THAT(IsNotNull(ReadStoredValue, TEXT("callable order readback function should be generated")));
		if (CallableThenHidden == nullptr || HiddenThenCallable == nullptr || PureThenHidden == nullptr
			|| HiddenThenPure == nullptr || HiddenExecCommand == nullptr || ReadStoredValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(CallableThenHidden->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("NotBlueprintCallable after BlueprintCallable should suppress FUNC_BlueprintCallable")));
		ASSERT_THAT(IsTrue(HiddenThenCallable->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("BlueprintCallable after NotBlueprintCallable should restore FUNC_BlueprintCallable")));
		ASSERT_THAT(IsFalse(CallableThenHidden->HasAnyFunctionFlags(FUNC_BlueprintPure),
			TEXT("callable suppression should not introduce pure flags")));
		ASSERT_THAT(IsFalse(HiddenThenCallable->HasAnyFunctionFlags(FUNC_BlueprintPure),
			TEXT("callable restoration should not introduce pure flags")));

		ASSERT_THAT(IsFalse(PureThenHidden->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("NotBlueprintCallable after BlueprintPure should suppress callable visibility")));
		ASSERT_THAT(IsTrue(PureThenHidden->HasAnyFunctionFlags(FUNC_BlueprintPure),
			TEXT("NotBlueprintCallable after BlueprintPure should keep FUNC_BlueprintPure")));
		ASSERT_THAT(IsTrue(PureThenHidden->HasAnyFunctionFlags(FUNC_Const),
			TEXT("const pure hidden function should keep FUNC_Const")));
		ASSERT_THAT(IsTrue(HiddenThenPure->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("BlueprintPure after NotBlueprintCallable should restore callable visibility")));
		ASSERT_THAT(IsTrue(HiddenThenPure->HasAnyFunctionFlags(FUNC_BlueprintPure),
			TEXT("BlueprintPure after NotBlueprintCallable should set FUNC_BlueprintPure")));
		ASSERT_THAT(IsTrue(HiddenThenPure->HasAnyFunctionFlags(FUNC_Const),
			TEXT("const pure restored function should keep FUNC_Const")));

		ASSERT_THAT(IsTrue(HiddenExecCommand->HasAnyFunctionFlags(FUNC_Exec),
			TEXT("NotBlueprintCallable Exec should still carry FUNC_Exec")));
		ASSERT_THAT(IsFalse(HiddenExecCommand->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("NotBlueprintCallable Exec should not be BlueprintCallable")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("callable specifier order actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		struct FVoidCallCase
		{
			const TCHAR* FunctionName;
			int32 InputValue;
			int32 ExpectedStoredValue;
		};

		const FVoidCallCase VoidCallCases[] = {
			{ TEXT("CallableThenHidden"), 10, 11 },
			{ TEXT("HiddenThenCallable"), 20, 22 },
			{ TEXT("HiddenExecCommand"), 30, 35 },
		};

		for (const FVoidCallCase& CallCase : VoidCallCases)
		{
			FFunctionInvoker ActionInvoker(*TestRunner, Actor, CallCase.FunctionName);
			const FString ValidMessage = FString::Printf(TEXT("%s should remain reflectively invokable"), CallCase.FunctionName);
			ASSERT_THAT(IsTrue(ActionInvoker.IsValid(), *ValidMessage));
			if (!ActionInvoker.IsValid())
			{
				return;
			}
			ActionInvoker.AddParam<int32>(CallCase.InputValue);
			const FString CallMessage = FString::Printf(TEXT("%s should execute through reflection"), CallCase.FunctionName);
			ASSERT_THAT(IsTrue(ActionInvoker.Call(), *CallMessage));

			FFunctionInvoker ReadInvoker(*TestRunner, Actor, TEXT("ReadStoredValue"));
			ASSERT_THAT(IsTrue(ReadInvoker.IsValid(), TEXT("ReadStoredValue should be invokable after callable-order action")));
			if (!ReadInvoker.IsValid())
			{
				return;
			}
			const FString ResultMessage = FString::Printf(TEXT("%s should update StoredValue"), CallCase.FunctionName);
			ASSERT_THAT(AreEqual(CallCase.ExpectedStoredValue, ReadInvoker.CallAndReturn<int32>(INDEX_NONE),
				*ResultMessage));
		}

		FFunctionInvoker PureThenHiddenInvoker(*TestRunner, Actor, TEXT("PureThenHidden"));
		ASSERT_THAT(IsTrue(PureThenHiddenInvoker.IsValid(), TEXT("PureThenHidden should be reflectively invokable")));
		if (!PureThenHiddenInvoker.IsValid())
		{
			return;
		}
		PureThenHiddenInvoker.AddParam<int32>(4);
		ASSERT_THAT(AreEqual(42, PureThenHiddenInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("PureThenHidden should execute using the latest StoredValue")));

		FFunctionInvoker HiddenThenPureInvoker(*TestRunner, Actor, TEXT("HiddenThenPure"));
		ASSERT_THAT(IsTrue(HiddenThenPureInvoker.IsValid(), TEXT("HiddenThenPure should be reflectively invokable")));
		if (!HiddenThenPureInvoker.IsValid())
		{
			return;
		}
		HiddenThenPureInvoker.AddParam<int32>(5);
		ASSERT_THAT(AreEqual(44, HiddenThenPureInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("HiddenThenPure should execute using the latest StoredValue")));
	}

	TEST_METHOD(AccessModifierFunctionFlagMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_AccessModifierFlags"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionAccessActor : AActor
			{
				UPROPERTY()
				int StoredValue = 0;

				UFUNCTION(BlueprintCallable, Category="Coverage|Access")
				private void PrivateCallable(int Value)
				{
					StoredValue += Value;
				}

				UFUNCTION(BlueprintPure, Category="Coverage|Access")
				private int PrivatePureValue() const
				{
					return StoredValue + 1;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Access")
				protected void ProtectedCallable(int Value)
				{
					StoredValue += Value * 10;
				}

				UFUNCTION(BlueprintPure, Category="Coverage|Access")
				protected int ProtectedPureValue() const
				{
					return StoredValue + 2;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Access")
				void PublicCallable(int Value)
				{
					StoredValue += Value * 100;
				}

				UFUNCTION(BlueprintPure, Category="Coverage|Access")
				int PublicPureValue() const
				{
					return StoredValue + 3;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Access")
				int CallAccessMatrix()
				{
					PrivateCallable(1);
					ProtectedCallable(2);
					PublicCallable(3);
					return PrivatePureValue() + ProtectedPureValue() + PublicPureValue();
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionAccessModifierFlags.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionAccessActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UFUNCTION access-modifier actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* PrivateCallable = FindFunctionForTest(ScriptClass, TEXT("PrivateCallable"));
		UFunction* PrivatePureValue = FindFunctionForTest(ScriptClass, TEXT("PrivatePureValue"));
		UFunction* ProtectedCallable = FindFunctionForTest(ScriptClass, TEXT("ProtectedCallable"));
		UFunction* ProtectedPureValue = FindFunctionForTest(ScriptClass, TEXT("ProtectedPureValue"));
		UFunction* PublicCallable = FindFunctionForTest(ScriptClass, TEXT("PublicCallable"));
		UFunction* PublicPureValue = FindFunctionForTest(ScriptClass, TEXT("PublicPureValue"));
		UFunction* CallAccessMatrix = FindFunctionForTest(ScriptClass, TEXT("CallAccessMatrix"));
		ASSERT_THAT(IsNotNull(PrivateCallable, TEXT("private BlueprintCallable UFUNCTION should still be generated")));
		ASSERT_THAT(IsNotNull(PrivatePureValue, TEXT("private BlueprintPure UFUNCTION should still be generated")));
		ASSERT_THAT(IsNotNull(ProtectedCallable, TEXT("protected BlueprintCallable UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ProtectedPureValue, TEXT("protected BlueprintPure UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(PublicCallable, TEXT("public BlueprintCallable UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(PublicPureValue, TEXT("public BlueprintPure UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(CallAccessMatrix, TEXT("public access-matrix wrapper UFUNCTION should be generated")));
		if (PrivateCallable == nullptr || PrivatePureValue == nullptr || ProtectedCallable == nullptr
			|| ProtectedPureValue == nullptr || PublicCallable == nullptr || PublicPureValue == nullptr || CallAccessMatrix == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(PrivateCallable->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("private BlueprintCallable UFUNCTION should suppress FUNC_BlueprintCallable")));
		ASSERT_THAT(IsFalse(PrivatePureValue->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure),
			TEXT("private BlueprintPure UFUNCTION should suppress BlueprintCallable and BlueprintPure flags")));
		ASSERT_THAT(IsTrue(PrivatePureValue->HasAnyFunctionFlags(FUNC_Const),
			TEXT("private const UFUNCTION should keep FUNC_Const")));
		ASSERT_THAT(IsFalse(PrivateCallable->HasMetaData(TEXT("BlueprintProtected")),
			TEXT("private UFUNCTION should not gain BlueprintProtected metadata")));
		ASSERT_THAT(IsTrue(ProtectedCallable->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("protected BlueprintCallable UFUNCTION should remain BlueprintCallable")));
		ASSERT_THAT(IsTrue(ProtectedCallable->HasMetaData(TEXT("BlueprintProtected")),
			TEXT("protected BlueprintCallable UFUNCTION should gain BlueprintProtected metadata")));
		ASSERT_THAT(IsTrue(HasAllFunctionFlags(ProtectedPureValue, FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_Const),
			TEXT("protected BlueprintPure const UFUNCTION should keep callable, pure, and const flags")));
		ASSERT_THAT(IsTrue(ProtectedPureValue->HasMetaData(TEXT("BlueprintProtected")),
			TEXT("protected BlueprintPure UFUNCTION should gain BlueprintProtected metadata")));
		ASSERT_THAT(IsTrue(PublicCallable->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("public BlueprintCallable UFUNCTION should keep FUNC_BlueprintCallable")));
		ASSERT_THAT(IsTrue(HasAllFunctionFlags(PublicPureValue, FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_Const),
			TEXT("public BlueprintPure const UFUNCTION should keep callable, pure, and const flags")));
		ASSERT_THAT(IsFalse(PublicCallable->HasMetaData(TEXT("BlueprintProtected")),
			TEXT("public UFUNCTION should not gain BlueprintProtected metadata")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UFUNCTION access-modifier actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker AccessInvoker(*TestRunner, Actor, TEXT("CallAccessMatrix"));
		ASSERT_THAT(IsTrue(AccessInvoker.IsValid(), TEXT("access-matrix wrapper should be reflectively invokable")));
		if (!AccessInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(969, AccessInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("script-side calls should reach private, protected, and public UFUNCTION implementations")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StoredValue"), 321,
			TEXT("access-matrix calls should update shared state through all access levels"))));
	}

	TEST_METHOD(PropertyAccessorCallbackUFunctionMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_PropertyAccessorCallbacks"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionAccessorActor : AActor
			{
				UPROPERTY(BlueprintReadWrite, BlueprintGetter=GetAccessorValue, BlueprintSetter=SetAccessorValue)
				int AccessorValue = 20;

				UPROPERTY(BlueprintReadOnly, BlueprintGetter=GetReadonlyValue)
				int ReadonlyValue = 7;

				UPROPERTY()
				int SetterCallCount = 0;

				UFUNCTION(BlueprintPure, Category="Coverage|Accessor")
				int GetAccessorValue() const
				{
					return AccessorValue;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Accessor")
				void SetAccessorValue(int NewValue)
				{
					SetterCallCount += 1;
					AccessorValue = NewValue;
				}

				UFUNCTION(BlueprintPure, Category="Coverage|Accessor")
				int GetReadonlyValue() const
				{
					return ReadonlyValue + AccessorValue;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionPropertyAccessorCallbacks.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionAccessorActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("property accessor callback actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FProperty* AccessorValue = ScriptClass->FindPropertyByName(TEXT("AccessorValue"));
		FProperty* ReadonlyValue = ScriptClass->FindPropertyByName(TEXT("ReadonlyValue"));
		UFunction* GetterFunction = FindFunctionForTest(ScriptClass, TEXT("GetAccessorValue"));
		UFunction* SetterFunction = FindFunctionForTest(ScriptClass, TEXT("SetAccessorValue"));
		UFunction* ReadonlyGetterFunction = FindFunctionForTest(ScriptClass, TEXT("GetReadonlyValue"));
		ASSERT_THAT(IsNotNull(AccessorValue, TEXT("BlueprintGetter/Setter property should be generated")));
		ASSERT_THAT(IsNotNull(ReadonlyValue, TEXT("BlueprintGetter-only property should be generated")));
		ASSERT_THAT(IsNotNull(GetterFunction, TEXT("BlueprintGetter callback should generate a UFUNCTION")));
		ASSERT_THAT(IsNotNull(SetterFunction, TEXT("BlueprintSetter callback should generate a UFUNCTION")));
		ASSERT_THAT(IsNotNull(ReadonlyGetterFunction, TEXT("BlueprintGetter-only callback should generate a UFUNCTION")));
		if (AccessorValue == nullptr || ReadonlyValue == nullptr || GetterFunction == nullptr || SetterFunction == nullptr || ReadonlyGetterFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("GetAccessorValue")), AccessorValue->GetMetaData(TEXT("BlueprintGetter")),
			TEXT("BlueprintGetter metadata should point at the reflected getter UFUNCTION")));
		ASSERT_THAT(AreEqual(FString(TEXT("SetAccessorValue")), AccessorValue->GetMetaData(TEXT("BlueprintSetter")),
			TEXT("BlueprintSetter metadata should point at the reflected setter UFUNCTION")));
		ASSERT_THAT(AreEqual(FString(TEXT("GetReadonlyValue")), ReadonlyValue->GetMetaData(TEXT("BlueprintGetter")),
			TEXT("BlueprintGetter-only metadata should point at the reflected getter UFUNCTION")));
		ASSERT_THAT(IsTrue(AccessorValue->HasAnyPropertyFlags(CPF_BlueprintVisible),
			TEXT("BlueprintGetter/Setter property should be Blueprint-visible")));
		ASSERT_THAT(IsFalse(AccessorValue->HasAnyPropertyFlags(CPF_BlueprintReadOnly),
			TEXT("BlueprintReadWrite accessor property should not be BlueprintReadOnly")));
		ASSERT_THAT(IsTrue(ReadonlyValue->HasAllPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly),
			TEXT("BlueprintReadOnly accessor property should remain read-only")));

		FIntProperty* GetterReturn = CastField<FIntProperty>(GetterFunction->GetReturnProperty());
		FIntProperty* SetterValueParam = CastField<FIntProperty>(FindParameterForTest(SetterFunction, TEXT("NewValue")));
		FIntProperty* ReadonlyGetterReturn = CastField<FIntProperty>(ReadonlyGetterFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(GetterReturn, TEXT("BlueprintGetter UFUNCTION should return int")));
		ASSERT_THAT(IsNotNull(SetterValueParam, TEXT("BlueprintSetter UFUNCTION should expose its int parameter")));
		ASSERT_THAT(IsNotNull(ReadonlyGetterReturn, TEXT("BlueprintGetter-only UFUNCTION should return int")));
		if (GetterReturn == nullptr || SetterValueParam == nullptr || ReadonlyGetterReturn == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(1, GetterFunction->NumParms,
			TEXT("BlueprintGetter UFUNCTION should expose only its return parameter")));
		ASSERT_THAT(AreEqual(1, SetterFunction->NumParms,
			TEXT("BlueprintSetter UFUNCTION should expose exactly one input parameter")));
		ASSERT_THAT(IsTrue(HasAllFunctionFlags(GetterFunction, FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_Const),
			TEXT("BlueprintGetter UFUNCTION should be callable, pure, and const")));
		ASSERT_THAT(IsTrue(SetterFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("BlueprintSetter UFUNCTION should be BlueprintCallable")));
		ASSERT_THAT(IsFalse(SetterFunction->HasAnyFunctionFlags(FUNC_BlueprintPure | FUNC_Const),
			TEXT("BlueprintSetter UFUNCTION should not be pure or const")));
		ASSERT_THAT(IsTrue(HasAllFunctionFlags(ReadonlyGetterFunction, FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_Const),
			TEXT("BlueprintGetter-only UFUNCTION should be callable, pure, and const")));
		ASSERT_THAT(IsTrue(GetterReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("BlueprintGetter return property should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(SetterValueParam->HasAnyPropertyFlags(CPF_Parm),
			TEXT("BlueprintSetter value property should be a parameter")));
		ASSERT_THAT(IsFalse(SetterValueParam->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm),
			TEXT("BlueprintSetter value property should be an ordinary input parameter")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("property accessor callback actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker InitialGetterInvoker(*TestRunner, Actor, TEXT("GetAccessorValue"));
		ASSERT_THAT(IsTrue(InitialGetterInvoker.IsValid(), TEXT("BlueprintGetter callback should be invokable")));
		if (!InitialGetterInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(20, InitialGetterInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("BlueprintGetter callback should read the initial property value")));

		FFunctionInvoker SetterInvoker(*TestRunner, Actor, TEXT("SetAccessorValue"));
		ASSERT_THAT(IsTrue(SetterInvoker.IsValid(), TEXT("BlueprintSetter callback should be invokable")));
		if (!SetterInvoker.IsValid())
		{
			return;
		}
		SetterInvoker.AddParam<int32>(35);
		ASSERT_THAT(IsTrue(SetterInvoker.Call(), TEXT("BlueprintSetter callback should execute")));

		FFunctionInvoker UpdatedGetterInvoker(*TestRunner, Actor, TEXT("GetAccessorValue"));
		ASSERT_THAT(IsTrue(UpdatedGetterInvoker.IsValid(), TEXT("BlueprintGetter callback should stay invokable after setter call")));
		if (!UpdatedGetterInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(35, UpdatedGetterInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("BlueprintSetter callback should update the value returned by the getter")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SetterCallCount"), 1,
			TEXT("BlueprintSetter callback should update script-side call count"))));

		FFunctionInvoker ReadonlyGetterInvoker(*TestRunner, Actor, TEXT("GetReadonlyValue"));
		ASSERT_THAT(IsTrue(ReadonlyGetterInvoker.IsValid(), TEXT("BlueprintGetter-only callback should be invokable")));
		if (!ReadonlyGetterInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, ReadonlyGetterInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("BlueprintGetter-only callback should execute against updated state")));
	}

	TEST_METHOD(ThreadSafeDispatchSubclassMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_ThreadSafeDispatch"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUFunctionThreadSafeObject : UObject
			{
				UFUNCTION()
				int FastReturn()
				{
					return 10;
				}

				UFUNCTION(meta=(BlueprintThreadSafe))
				int FunctionThreadSafeReturn()
				{
					return 20;
				}
			}

			UCLASS(meta=(BlueprintThreadSafe))
			class UCoverageUFunctionThreadSafeClassObject : UObject
			{
				UFUNCTION()
				int ClassThreadSafeReturn()
				{
					return 30;
				}

				UFUNCTION(meta=(NotBlueprintThreadSafe))
				int ClassThreadSafeOptOutReturn()
				{
					return 40;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionThreadSafeDispatch.as"),
			ScriptSource,
			TEXT("UCoverageUFunctionThreadSafeObject"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("thread-safe dispatch object should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UClass* ThreadSafeClass = FindGeneratedClass(&Engine, TEXT("UCoverageUFunctionThreadSafeClassObject"));
		ASSERT_THAT(IsNotNull(ThreadSafeClass, TEXT("class-level thread-safe object should be generated")));
		if (ThreadSafeClass == nullptr)
		{
			return;
		}

		UASFunction* FastReturn = Cast<UASFunction>(FindFunctionForTest(ScriptClass, TEXT("FastReturn")));
		UASFunction* FunctionThreadSafeReturn = Cast<UASFunction>(FindFunctionForTest(ScriptClass, TEXT("FunctionThreadSafeReturn")));
		UASFunction* ClassThreadSafeReturn = Cast<UASFunction>(FindFunctionForTest(ThreadSafeClass, TEXT("ClassThreadSafeReturn")));
		UASFunction* ClassThreadSafeOptOutReturn = Cast<UASFunction>(FindFunctionForTest(ThreadSafeClass, TEXT("ClassThreadSafeOptOutReturn")));
		ASSERT_THAT(IsNotNull(FastReturn, TEXT("ordinary UFUNCTION should generate UASFunction subclass")));
		ASSERT_THAT(IsNotNull(FunctionThreadSafeReturn, TEXT("function-level thread-safe UFUNCTION should generate UASFunction")));
		ASSERT_THAT(IsNotNull(ClassThreadSafeReturn, TEXT("class-level thread-safe UFUNCTION should generate UASFunction")));
		ASSERT_THAT(IsNotNull(ClassThreadSafeOptOutReturn, TEXT("NotBlueprintThreadSafe UFUNCTION should generate UASFunction subclass")));
		if (FastReturn == nullptr || FunctionThreadSafeReturn == nullptr || ClassThreadSafeReturn == nullptr || ClassThreadSafeOptOutReturn == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(IsFunctionClassOneOf(FastReturn, UASFunction_DWordReturn::StaticClass(), UASFunction_DWordReturn_JIT::StaticClass()),
			TEXT("ordinary primitive return should use the optimized non-thread-safe return wrapper")));
		ASSERT_THAT(IsTrue(IsFunctionClassOneOf(FunctionThreadSafeReturn, UASFunction::StaticClass(), UASFunction_JIT::StaticClass()),
			TEXT("function-level BlueprintThreadSafe should force the generic thread-safe UASFunction wrapper")));
		ASSERT_THAT(IsTrue(IsFunctionClassOneOf(ClassThreadSafeReturn, UASFunction::StaticClass(), UASFunction_JIT::StaticClass()),
			TEXT("class-level BlueprintThreadSafe should force the generic thread-safe UASFunction wrapper")));
		ASSERT_THAT(IsTrue(IsFunctionClassOneOf(ClassThreadSafeOptOutReturn, UASFunction_DWordReturn::StaticClass(), UASFunction_DWordReturn_JIT::StaticClass()),
			TEXT("NotBlueprintThreadSafe should opt back into the optimized non-thread-safe return wrapper")));
		ASSERT_THAT(IsTrue(FunctionThreadSafeReturn->HasMetaData(TEXT("BlueprintThreadSafe")),
			TEXT("function-level BlueprintThreadSafe metadata should round-trip")));
		ASSERT_THAT(IsTrue(ThreadSafeClass->HasMetaData(TEXT("BlueprintThreadSafe")),
			TEXT("class-level BlueprintThreadSafe metadata should round-trip")));
		ASSERT_THAT(IsTrue(ClassThreadSafeOptOutReturn->HasMetaData(TEXT("NotBlueprintThreadSafe")),
			TEXT("NotBlueprintThreadSafe metadata should round-trip")));

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		UObject* ThreadSafeInstance = NewObject<UObject>(GetTransientPackage(), ThreadSafeClass);
		ASSERT_THAT(IsNotNull(Instance, TEXT("thread-safe dispatch object should instantiate")));
		ASSERT_THAT(IsNotNull(ThreadSafeInstance, TEXT("class-level thread-safe object should instantiate")));
		if (Instance == nullptr || ThreadSafeInstance == nullptr)
		{
			return;
		}

		FFunctionInvoker FastInvoker(*TestRunner, Instance, TEXT("FastReturn"));
		ASSERT_THAT(IsTrue(FastInvoker.IsValid(), TEXT("ordinary UFUNCTION dispatch should be invokable")));
		if (!FastInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(10, FastInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("ordinary optimized dispatch should preserve return value")));

		FFunctionInvoker FunctionThreadSafeInvoker(*TestRunner, Instance, TEXT("FunctionThreadSafeReturn"));
		ASSERT_THAT(IsTrue(FunctionThreadSafeInvoker.IsValid(), TEXT("function-level thread-safe UFUNCTION should be invokable")));
		if (!FunctionThreadSafeInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(20, FunctionThreadSafeInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("function-level thread-safe dispatch should preserve return value")));

		FFunctionInvoker ClassThreadSafeInvoker(*TestRunner, ThreadSafeInstance, TEXT("ClassThreadSafeReturn"));
		ASSERT_THAT(IsTrue(ClassThreadSafeInvoker.IsValid(), TEXT("class-level thread-safe UFUNCTION should be invokable")));
		if (!ClassThreadSafeInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(30, ClassThreadSafeInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("class-level thread-safe dispatch should preserve return value")));

		FFunctionInvoker OptOutInvoker(*TestRunner, ThreadSafeInstance, TEXT("ClassThreadSafeOptOutReturn"));
		ASSERT_THAT(IsTrue(OptOutInvoker.IsValid(), TEXT("NotBlueprintThreadSafe UFUNCTION should be invokable")));
		if (!OptOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(40, OptOutInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("NotBlueprintThreadSafe dispatch should preserve return value")));
	}

	TEST_METHOD(FunctionDispatchSubclassShapeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_DispatchSubclassShapes"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionDispatchShapeActor : AActor
			{
				UPROPERTY()
				int StoredValue = 0;

				UFUNCTION()
				void ReturnVoid()
				{
					StoredValue = 1;
				}

				UFUNCTION()
				bool ReturnBool()
				{
					return true;
				}

				UFUNCTION()
				uint8 ReturnByte()
				{
					return 42;
				}

				UFUNCTION()
				int ReturnInt()
				{
					return 42;
				}

				UFUNCTION()
				float ReturnFloat()
				{
					return 42.0f;
				}

				UFUNCTION()
				double ReturnDouble()
				{
					return 42.0;
				}

				UFUNCTION()
				AActor ReturnObject()
				{
					return this;
				}

				UFUNCTION()
				FString ReturnString()
				{
					return "shape";
				}

				UFUNCTION()
				void AcceptInt(int Value)
				{
					StoredValue = Value;
				}

				UFUNCTION()
				void AcceptDouble(double Value)
				{
					StoredValue = int(Value);
				}

				UFUNCTION()
				void AcceptByte(uint8 Value)
				{
					StoredValue = int(Value);
				}

				UFUNCTION()
				void AcceptRef(int&out Value)
				{
					Value = 42;
					StoredValue = Value;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionDispatchSubclassShapes.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionDispatchShapeActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("dispatch subclass shape actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		struct FExpectedFunctionClass
		{
			const TCHAR* FunctionName;
			const UClass* ExpectedClass;
			const UClass* ExpectedJitClass;
		};

		const FExpectedFunctionClass ExpectedClasses[] = {
			{ TEXT("ReturnVoid"), UASFunction_NotThreadSafe::StaticClass(), UASFunction_NotThreadSafe_JIT::StaticClass() },
			{ TEXT("ReturnBool"), UASFunction_ByteReturn::StaticClass(), UASFunction_ByteReturn_JIT::StaticClass() },
			{ TEXT("ReturnByte"), UASFunction_ByteReturn::StaticClass(), UASFunction_ByteReturn_JIT::StaticClass() },
			{ TEXT("ReturnInt"), UASFunction_DWordReturn::StaticClass(), UASFunction_DWordReturn_JIT::StaticClass() },
			{ TEXT("ReturnFloat"), UASFunction_FloatReturn::StaticClass(), UASFunction_FloatReturn_JIT::StaticClass() },
			{ TEXT("ReturnDouble"), UASFunction_DoubleReturn::StaticClass(), UASFunction_DoubleReturn_JIT::StaticClass() },
			{ TEXT("ReturnObject"), UASFunction_ObjectReturn::StaticClass(), UASFunction_ObjectReturn_JIT::StaticClass() },
			{ TEXT("ReturnString"), UASFunction_NotThreadSafe::StaticClass(), UASFunction_NotThreadSafe_JIT::StaticClass() },
			{ TEXT("AcceptInt"), UASFunction_DWordArg::StaticClass(), UASFunction_DWordArg_JIT::StaticClass() },
			{ TEXT("AcceptDouble"), UASFunction_DoubleArg::StaticClass(), UASFunction_DoubleArg_JIT::StaticClass() },
			{ TEXT("AcceptByte"), UASFunction_ByteArg::StaticClass(), UASFunction_ByteArg_JIT::StaticClass() },
			{ TEXT("AcceptRef"), UASFunction_ReferenceArg::StaticClass(), UASFunction_ReferenceArg_JIT::StaticClass() },
		};

		for (const FExpectedFunctionClass& ExpectedClass : ExpectedClasses)
		{
			UFunction* Function = FindFunctionForTest(ScriptClass, ExpectedClass.FunctionName);
			const FString GeneratedMessage = FString::Printf(TEXT("%s should be generated"), ExpectedClass.FunctionName);
			ASSERT_THAT(IsNotNull(Function, *GeneratedMessage));
			if (Function == nullptr)
			{
				return;
			}

			const FString ClassMessage = FString::Printf(TEXT("%s should select the expected UASFunction subclass"), ExpectedClass.FunctionName);
			ASSERT_THAT(IsTrue(IsFunctionClassOneOf(Function, ExpectedClass.ExpectedClass, ExpectedClass.ExpectedJitClass),
				*ClassMessage));
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("dispatch subclass shape actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker VoidInvoker(*TestRunner, Actor, TEXT("ReturnVoid"));
		ASSERT_THAT(IsTrue(VoidInvoker.IsValid(), TEXT("ReturnVoid should be invokable")));
		if (!VoidInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(VoidInvoker.Call(), TEXT("ReturnVoid should execute through generic void path")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StoredValue"), 1,
			TEXT("ReturnVoid should update state through its body"))));

		FFunctionInvoker BoolInvoker(*TestRunner, Actor, TEXT("ReturnBool"));
		ASSERT_THAT(IsTrue(BoolInvoker.IsValid(), TEXT("ReturnBool should be invokable")));
		if (!BoolInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(BoolInvoker.CallAndReturn<bool>(false),
			TEXT("bool return should execute through byte-return optimized subclass")));

		FFunctionInvoker ByteInvoker(*TestRunner, Actor, TEXT("ReturnByte"));
		ASSERT_THAT(IsTrue(ByteInvoker.IsValid(), TEXT("ReturnByte should be invokable")));
		if (!ByteInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<uint8>(42), ByteInvoker.CallAndReturn<uint8>(0),
			TEXT("uint8 return should execute through byte-return optimized subclass")));

		FFunctionInvoker IntInvoker(*TestRunner, Actor, TEXT("ReturnInt"));
		ASSERT_THAT(IsTrue(IntInvoker.IsValid(), TEXT("ReturnInt should be invokable")));
		if (!IntInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, IntInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("int return should execute through dword-return optimized subclass")));

		FFunctionInvoker FloatInvoker(*TestRunner, Actor, TEXT("ReturnFloat"));
		ASSERT_THAT(IsTrue(FloatInvoker.IsValid(), TEXT("ReturnFloat should be invokable")));
		if (!FloatInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(42.0f, FloatInvoker.CallAndReturn<float>(0.0f)),
			TEXT("float return should execute through float-return optimized subclass")));

		FFunctionInvoker DoubleInvoker(*TestRunner, Actor, TEXT("ReturnDouble"));
		ASSERT_THAT(IsTrue(DoubleInvoker.IsValid(), TEXT("ReturnDouble should be invokable")));
		if (!DoubleInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(42.0, DoubleInvoker.CallAndReturn<double>(0.0)),
			TEXT("double return should execute through double-return optimized subclass")));

		FFunctionInvoker ObjectInvoker(*TestRunner, Actor, TEXT("ReturnObject"));
		ASSERT_THAT(IsTrue(ObjectInvoker.IsValid(), TEXT("ReturnObject should be invokable")));
		if (!ObjectInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(Actor, ObjectInvoker.CallAndReturn<AActor*>(nullptr),
			TEXT("object return should execute through object-return optimized subclass")));

		FFunctionInvoker StringInvoker(*TestRunner, Actor, TEXT("ReturnString"));
		ASSERT_THAT(IsTrue(StringInvoker.IsValid(), TEXT("ReturnString should be invokable")));
		if (!StringInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("shape")), StringInvoker.CallAndReturn<FString>(FString()),
			TEXT("FString return should execute through generic fallback subclass")));

		FFunctionInvoker IntArgInvoker(*TestRunner, Actor, TEXT("AcceptInt"));
		ASSERT_THAT(IsTrue(IntArgInvoker.IsValid(), TEXT("AcceptInt should be invokable")));
		if (!IntArgInvoker.IsValid())
		{
			return;
		}
		IntArgInvoker.AddParam<int32>(39);
		ASSERT_THAT(IsTrue(IntArgInvoker.Call(), TEXT("AcceptInt should execute through dword-arg optimized subclass")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StoredValue"), 39,
			TEXT("AcceptInt should update StoredValue"))));

		FFunctionInvoker DoubleArgInvoker(*TestRunner, Actor, TEXT("AcceptDouble"));
		ASSERT_THAT(IsTrue(DoubleArgInvoker.IsValid(), TEXT("AcceptDouble should be invokable")));
		if (!DoubleArgInvoker.IsValid())
		{
			return;
		}
		DoubleArgInvoker.AddParam<double>(40.0);
		ASSERT_THAT(IsTrue(DoubleArgInvoker.Call(), TEXT("AcceptDouble should execute through double-arg optimized subclass")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StoredValue"), 40,
			TEXT("AcceptDouble should update StoredValue"))));

		FFunctionInvoker ByteArgInvoker(*TestRunner, Actor, TEXT("AcceptByte"));
		ASSERT_THAT(IsTrue(ByteArgInvoker.IsValid(), TEXT("AcceptByte should be invokable")));
		if (!ByteArgInvoker.IsValid())
		{
			return;
		}
		ByteArgInvoker.AddParam<uint8>(41);
		ASSERT_THAT(IsTrue(ByteArgInvoker.Call(), TEXT("AcceptByte should execute through byte-arg optimized subclass")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StoredValue"), 41,
			TEXT("AcceptByte should update StoredValue"))));

		FFunctionInvoker RefArgInvoker(*TestRunner, Actor, TEXT("AcceptRef"));
		ASSERT_THAT(IsTrue(RefArgInvoker.IsValid(), TEXT("AcceptRef should be invokable")));
		if (!RefArgInvoker.IsValid())
		{
			return;
		}
		FProperty* RefSlotProperty = nullptr;
		void* RefSlot = nullptr;
		ASSERT_THAT(IsTrue(RefArgInvoker.AddParamSlot(RefSlotProperty, RefSlot),
			TEXT("AcceptRef should expose an out-ref parameter slot")));
		FIntProperty* RefIntProperty = CastField<FIntProperty>(RefSlotProperty);
		ASSERT_THAT(IsNotNull(RefIntProperty, TEXT("AcceptRef out-ref slot should be FIntProperty")));
		if (RefSlot == nullptr || RefIntProperty == nullptr)
		{
			return;
		}
		RefIntProperty->SetPropertyValue(RefSlot, 0);
		ASSERT_THAT(IsTrue(RefArgInvoker.Call(), TEXT("AcceptRef should execute through reference-arg optimized subclass")));
		ASSERT_THAT(AreEqual(42, RefIntProperty->GetPropertyValue(RefSlot),
			TEXT("AcceptRef should write the out-ref caller buffer")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StoredValue"), 42,
			TEXT("AcceptRef should update StoredValue"))));
	}

	TEST_METHOD(StaticGlobalFunctionReflectionAndRuntimeCall)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_StaticGlobalReflection"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UFUNCTION(BlueprintCallable, Category="Coverage|Global", meta=(WorldContext="WorldContextObject", DisplayName="Coverage Global Add"))
			int CoverageGlobalAdd(UObject WorldContextObject, int Value)
			{
				return WorldContextObject != nullptr ? Value + 34 : -1;
			}
			)AS");
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionStaticGlobalReflection.as"),
			ScriptSource);
		ASSERT_THAT(IsTrue(bCompiled, TEXT("global UFUNCTION module should compile")));
		if (!bCompiled)
		{
			return;
		}

		UClass* StaticsClass = FindGeneratedClass(&Engine, TEXT("UModule_ASCoverageUFunction_StaticGlobalReflectionStatics"));
		ASSERT_THAT(IsNotNull(StaticsClass, TEXT("global UFUNCTION should generate a module statics class")));
		if (StaticsClass == nullptr)
		{
			return;
		}

		UFunction* CoverageGlobalAdd = FindFunctionForTest(StaticsClass, TEXT("CoverageGlobalAdd"));
		ASSERT_THAT(IsNotNull(CoverageGlobalAdd, TEXT("global UFUNCTION should be generated on the statics class")));
		if (CoverageGlobalAdd == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(CoverageGlobalAdd->HasAnyFunctionFlags(FUNC_Static),
			TEXT("global UFUNCTION should set FUNC_Static")));
		ASSERT_THAT(IsTrue(CoverageGlobalAdd->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("global BlueprintCallable UFUNCTION should set FUNC_BlueprintCallable")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Global")), CoverageGlobalAdd->GetMetaData(TEXT("Category")),
			TEXT("global UFUNCTION Category metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("WorldContextObject")), CoverageGlobalAdd->GetMetaData(TEXT("WorldContext")),
			TEXT("global UFUNCTION WorldContext metadata should preserve the explicit parameter name")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage Global Add")), CoverageGlobalAdd->GetMetaData(TEXT("DisplayName")),
			TEXT("global UFUNCTION DisplayName metadata should be preserved")));

		FObjectProperty* WorldContextParam = CastField<FObjectProperty>(FindParameterForTest(CoverageGlobalAdd, TEXT("WorldContextObject")));
		FIntProperty* ValueParam = CastField<FIntProperty>(FindParameterForTest(CoverageGlobalAdd, TEXT("Value")));
		FIntProperty* ReturnValue = CastField<FIntProperty>(FindParameterForTest(CoverageGlobalAdd, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(WorldContextParam, TEXT("global UFUNCTION should reflect explicit UObject world-context parameter")));
		ASSERT_THAT(IsNotNull(ValueParam, TEXT("global UFUNCTION should reflect ordinary int parameter")));
		ASSERT_THAT(IsNotNull(ReturnValue, TEXT("global UFUNCTION should reflect int return value")));
		if (WorldContextParam == nullptr || ValueParam == nullptr || ReturnValue == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(UObject::StaticClass(), WorldContextParam->PropertyClass,
			TEXT("global world-context parameter should preserve UObject type")));
		ASSERT_THAT(IsTrue(ReturnValue->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("global UFUNCTION return value should carry CPF_ReturnParm")));

		UObject* StaticsDefaultObject = StaticsClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(StaticsDefaultObject, TEXT("global UFUNCTION statics class should have a default object for invocation")));
		if (StaticsDefaultObject == nullptr)
		{
			return;
		}

		FFunctionInvoker Invoker(*TestRunner, StaticsDefaultObject, TEXT("CoverageGlobalAdd"));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("global UFUNCTION should be invokable through the statics CDO")));
		if (!Invoker.IsValid())
		{
			return;
		}
		Invoker.AddParam<UObject*>(StaticsDefaultObject);
		Invoker.AddParam<int32>(8);
		ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("global UFUNCTION reflected invocation should execute through the generated statics class")));
	}

	TEST_METHOD(StaticWorldContextGenerationMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_StaticWorldContextGeneration"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UFUNCTION(BlueprintCallable, Category="Coverage|StaticWorld")
			int StaticNeedsGeneratedWorldContext(int Value)
			{
				return __WorldContext() != nullptr ? Value + 10 : -10;
			}

			UFUNCTION(BlueprintCallable, Category="Coverage|StaticWorld", meta=(WorldContext="WorldContextObject"))
			int StaticUsesExplicitWorldContext(AActor WorldContextObject, int Value)
			{
				if (__WorldContext() != WorldContextObject)
				{
					return -20;
				}

				UWorld CurrentWorld = GetCurrentWorld();
				if (CurrentWorld == nullptr || CurrentWorld != WorldContextObject.GetWorld())
				{
					return -30;
				}

				return Value + 20;
			}
			)AS");
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionStaticWorldContextGeneration.as"),
			ScriptSource);
		ASSERT_THAT(IsTrue(bCompiled, TEXT("static world-context UFUNCTION module should compile")));
		if (!bCompiled)
		{
			return;
		}

		UClass* StaticsClass = FindGeneratedClass(&Engine, TEXT("UModule_ASCoverageUFunction_StaticWorldContextGenerationStatics"));
		ASSERT_THAT(IsNotNull(StaticsClass, TEXT("static world-context UFUNCTIONs should generate a statics class")));
		if (StaticsClass == nullptr)
		{
			return;
		}

		UASFunction* GeneratedWorldFunction = Cast<UASFunction>(FindFunctionForTest(StaticsClass, TEXT("StaticNeedsGeneratedWorldContext")));
		UASFunction* ExplicitWorldFunction = Cast<UASFunction>(FindFunctionForTest(StaticsClass, TEXT("StaticUsesExplicitWorldContext")));
		ASSERT_THAT(IsNotNull(GeneratedWorldFunction, TEXT("static UFUNCTION without explicit WorldContext should generate a UASFunction")));
		ASSERT_THAT(IsNotNull(ExplicitWorldFunction, TEXT("static UFUNCTION with explicit WorldContext should generate a UASFunction")));
		if (GeneratedWorldFunction == nullptr || ExplicitWorldFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(GeneratedWorldFunction->HasAnyFunctionFlags(FUNC_Static),
			TEXT("generated-world-context UFUNCTION should be static")));
		ASSERT_THAT(IsTrue(ExplicitWorldFunction->HasAnyFunctionFlags(FUNC_Static),
			TEXT("explicit-world-context UFUNCTION should be static")));
		ASSERT_THAT(IsTrue(GeneratedWorldFunction->bIsWorldContextGenerated,
			TEXT("static UFUNCTION without WorldContext metadata should synthesize a world-context parameter")));
		ASSERT_THAT(IsFalse(ExplicitWorldFunction->bIsWorldContextGenerated,
			TEXT("static UFUNCTION with WorldContext metadata should not synthesize an extra parameter")));
		ASSERT_THAT(AreEqual(FString(TEXT("_World_Context")), GeneratedWorldFunction->GetMetaData(TEXT("WorldContext")),
			TEXT("generated world-context metadata should point at the synthetic parameter")));
		ASSERT_THAT(AreEqual(FString(TEXT("WorldContextObject")), ExplicitWorldFunction->GetMetaData(TEXT("WorldContext")),
			TEXT("explicit world-context metadata should preserve the declared parameter")));
		ASSERT_THAT(AreEqual(1, GeneratedWorldFunction->WorldContextIndex,
			TEXT("generated world-context argument should be appended after declared parameters")));
		ASSERT_THAT(AreEqual(0, ExplicitWorldFunction->WorldContextIndex,
			TEXT("explicit world-context argument should retain declared argument index")));

		FIntProperty* GeneratedValueParam = CastField<FIntProperty>(FindParameterForTest(GeneratedWorldFunction, TEXT("Value")));
		FObjectProperty* GeneratedWorldParam = CastField<FObjectProperty>(FindParameterForTest(GeneratedWorldFunction, TEXT("_World_Context")));
		FIntProperty* GeneratedReturn = CastField<FIntProperty>(FindParameterForTest(GeneratedWorldFunction, TEXT("ReturnValue")));
		FObjectProperty* ExplicitWorldParam = CastField<FObjectProperty>(FindParameterForTest(ExplicitWorldFunction, TEXT("WorldContextObject")));
		FIntProperty* ExplicitValueParam = CastField<FIntProperty>(FindParameterForTest(ExplicitWorldFunction, TEXT("Value")));
		FIntProperty* ExplicitReturn = CastField<FIntProperty>(FindParameterForTest(ExplicitWorldFunction, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(GeneratedValueParam, TEXT("generated-world-context UFUNCTION should reflect its declared value parameter")));
		ASSERT_THAT(IsNotNull(GeneratedWorldParam, TEXT("generated-world-context UFUNCTION should reflect the synthetic world-context parameter")));
		ASSERT_THAT(IsNotNull(GeneratedReturn, TEXT("generated-world-context UFUNCTION should reflect its return parameter")));
		ASSERT_THAT(IsNotNull(ExplicitWorldParam, TEXT("explicit-world-context UFUNCTION should reflect the declared world-context parameter")));
		ASSERT_THAT(IsNotNull(ExplicitValueParam, TEXT("explicit-world-context UFUNCTION should reflect its value parameter")));
		ASSERT_THAT(IsNotNull(ExplicitReturn, TEXT("explicit-world-context UFUNCTION should reflect its return parameter")));
		if (GeneratedValueParam == nullptr || GeneratedWorldParam == nullptr || GeneratedReturn == nullptr
			|| ExplicitWorldParam == nullptr || ExplicitValueParam == nullptr || ExplicitReturn == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(UObject::StaticClass(), GeneratedWorldParam->PropertyClass,
			TEXT("synthetic world-context parameter should be UObject")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), ExplicitWorldParam->PropertyClass,
			TEXT("explicit world-context parameter should preserve AActor type")));
		ASSERT_THAT(AreEqual(GeneratedWorldFunction->WorldContextOffsetInParms, GeneratedWorldParam->GetOffset_ForUFunction(),
			TEXT("generated world-context offset should match the reflected property offset")));
		ASSERT_THAT(AreEqual(ExplicitWorldFunction->WorldContextOffsetInParms, ExplicitWorldParam->GetOffset_ForUFunction(),
			TEXT("explicit world-context offset should match the reflected property offset")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(GeneratedWorldFunction), GeneratedWorldParam->GetOwner<UObject>(),
			TEXT("synthetic world-context parameter should still be owned by the generated UASFunction")));
		ASSERT_THAT(IsFalse(IsAngelscriptGenerated(GeneratedWorldParam),
			TEXT("synthetic world-context parameter is intentionally not part of the public generated-argument list")));
		ASSERT_THAT(IsFalse(IsAngelscriptWorldContextProperty(GeneratedWorldParam),
			TEXT("synthetic world-context parameter currently remains outside IsAngelscriptWorldContextProperty classification")));
		ASSERT_THAT(IsTrue(IsAngelscriptWorldContextProperty(ExplicitWorldParam),
			TEXT("explicit world-context parameter should be classified as a world-context property")));
		ASSERT_THAT(IsFalse(IsAngelscriptWorldContextProperty(GeneratedValueParam),
			TEXT("ordinary static parameter should not be classified as a world-context property")));
		ASSERT_THAT(IsFalse(IsAngelscriptWorldContextProperty(GeneratedReturn),
			TEXT("static return parameter should not be classified as a world-context property")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor& ContextActor = Spawner.SpawnActor<AActor>();
		UObject* StaticsDefaultObject = StaticsClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(StaticsDefaultObject, TEXT("statics class CDO should be available for static invocation")));
		if (StaticsDefaultObject == nullptr)
		{
			return;
		}

		FFunctionInvoker GeneratedInvoker(*TestRunner, StaticsDefaultObject, TEXT("StaticNeedsGeneratedWorldContext"));
		ASSERT_THAT(IsTrue(GeneratedInvoker.IsValid(), TEXT("generated-world-context UFUNCTION should be invokable")));
		if (!GeneratedInvoker.IsValid())
		{
			return;
		}
		GeneratedInvoker.AddParam<int32>(32);
		GeneratedInvoker.AddParam<UObject*>(&ContextActor);
		ASSERT_THAT(AreEqual(42, GeneratedInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("generated world-context parameter should be consumed by the static UFUNCTION thunk")));

		FFunctionInvoker ExplicitInvoker(*TestRunner, StaticsDefaultObject, TEXT("StaticUsesExplicitWorldContext"));
		ASSERT_THAT(IsTrue(ExplicitInvoker.IsValid(), TEXT("explicit-world-context UFUNCTION should be invokable")));
		if (!ExplicitInvoker.IsValid())
		{
			return;
		}
		ExplicitInvoker.AddParam<AActor*>(&ContextActor);
		ExplicitInvoker.AddParam<int32>(22);
		ASSERT_THAT(AreEqual(42, ExplicitInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("explicit world-context parameter should drive __WorldContext and GetCurrentWorld")));
	}

	TEST_METHOD(StaticGlobalComplexParameterMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_StaticGlobalComplexParameters"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FUFunctionGlobalPayload
			{
				UPROPERTY()
				int Count = 0;

				UPROPERTY()
				FString Label;
			}

			UFUNCTION(BlueprintCallable, Category="Coverage|GlobalComplex")
			int StaticScorePayload(UObject WorldContextObject, const FUFunctionGlobalPayload&in Payload, const TArray<int>&in Values)
			{
				int Sum = 0;
				for (int Value : Values)
				{
					Sum += Value;
				}

				return (WorldContextObject != nullptr ? 1 : 0) + Payload.Count + Payload.Label.Len() + Sum;
			}

			UFUNCTION(BlueprintCallable, Category="Coverage|GlobalComplex")
			void StaticFillPayload(UObject WorldContextObject, FUFunctionGlobalPayload&out OutPayload, int&out OutScore)
			{
				OutPayload.Count = WorldContextObject != nullptr ? 17 : -17;
				OutPayload.Label = "GlobalPayload";
				OutScore = OutPayload.Count + OutPayload.Label.Len();
			}

			UFUNCTION(BlueprintPure, Category="Coverage|GlobalComplex")
			FUFunctionGlobalPayload StaticReturnPayload(UObject WorldContextObject, int Value)
			{
				FUFunctionGlobalPayload Payload;
				Payload.Count = WorldContextObject != nullptr ? Value : -Value;
				Payload.Label = "ReturnedGlobal";
				return Payload;
			}
			)AS");
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionStaticGlobalComplexParameters.as"),
			ScriptSource);
		ASSERT_THAT(IsTrue(bCompiled, TEXT("static global complex UFUNCTION module should compile")));
		if (!bCompiled)
		{
			return;
		}

		UClass* StaticsClass = FindGeneratedClass(&Engine, TEXT("UModule_ASCoverageUFunction_StaticGlobalComplexParametersStatics"));
		ASSERT_THAT(IsNotNull(StaticsClass, TEXT("complex global UFUNCTIONs should generate a statics class")));
		if (StaticsClass == nullptr)
		{
			return;
		}

		UFunction* StaticScorePayload = FindFunctionForTest(StaticsClass, TEXT("StaticScorePayload"));
		UFunction* StaticFillPayload = FindFunctionForTest(StaticsClass, TEXT("StaticFillPayload"));
		UFunction* StaticReturnPayload = FindFunctionForTest(StaticsClass, TEXT("StaticReturnPayload"));
		ASSERT_THAT(IsNotNull(StaticScorePayload, TEXT("StaticScorePayload should be generated")));
		ASSERT_THAT(IsNotNull(StaticFillPayload, TEXT("StaticFillPayload should be generated")));
		ASSERT_THAT(IsNotNull(StaticReturnPayload, TEXT("StaticReturnPayload should be generated")));
		if (StaticScorePayload == nullptr || StaticFillPayload == nullptr || StaticReturnPayload == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(HasAllFunctionFlags(StaticScorePayload, FUNC_Static | FUNC_BlueprintCallable),
			TEXT("complex global callable should be static and BlueprintCallable")));
		ASSERT_THAT(IsTrue(HasAllFunctionFlags(StaticFillPayload, FUNC_Static | FUNC_BlueprintCallable),
			TEXT("complex global out-param function should be static and BlueprintCallable")));
		ASSERT_THAT(IsTrue(HasAllFunctionFlags(StaticReturnPayload, FUNC_Static | FUNC_BlueprintCallable | FUNC_BlueprintPure),
			TEXT("complex global pure function should be static, callable, and pure")));

		FObjectProperty* ScoreWorldParam = CastField<FObjectProperty>(FindParameterForTest(StaticScorePayload, TEXT("WorldContextObject")));
		FStructProperty* ScorePayloadParam = CastField<FStructProperty>(FindParameterForTest(StaticScorePayload, TEXT("Payload")));
		FArrayProperty* ScoreValuesParam = CastField<FArrayProperty>(FindParameterForTest(StaticScorePayload, TEXT("Values")));
		FIntProperty* ScoreReturnParam = CastField<FIntProperty>(StaticScorePayload->GetReturnProperty());
		ASSERT_THAT(IsNotNull(ScoreWorldParam, TEXT("StaticScorePayload should reflect explicit world-context object")));
		ASSERT_THAT(IsNotNull(ScorePayloadParam, TEXT("StaticScorePayload should reflect AS struct const-ref parameter")));
		ASSERT_THAT(IsNotNull(ScoreValuesParam, TEXT("StaticScorePayload should reflect TArray<int> const-ref parameter")));
		ASSERT_THAT(IsNotNull(ScoreReturnParam, TEXT("StaticScorePayload should reflect int return")));
		if (ScoreWorldParam == nullptr || ScorePayloadParam == nullptr || ScoreValuesParam == nullptr || ScoreReturnParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(UObject::StaticClass(), ScoreWorldParam->PropertyClass,
			TEXT("StaticScorePayload world-context parameter should preserve UObject")));
		ASSERT_THAT(IsTrue(ScorePayloadParam->HasAllPropertyFlags(CPF_ConstParm | CPF_OutParm | CPF_ReferenceParm),
			TEXT("global AS struct const-ref parameter should carry const/out/reference flags")));
		ASSERT_THAT(IsTrue(ScoreValuesParam->HasAllPropertyFlags(CPF_ConstParm | CPF_OutParm | CPF_ReferenceParm),
			TEXT("global TArray<int> const-ref parameter should carry const/out/reference flags")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(ScoreValuesParam->Inner),
			TEXT("global TArray<int> parameter inner should be FIntProperty")));
		ASSERT_THAT(IsTrue(ScoreReturnParam->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("global score return should carry CPF_ReturnParm")));

		FStructProperty* FillOutPayloadParam = CastField<FStructProperty>(FindParameterForTest(StaticFillPayload, TEXT("OutPayload")));
		FIntProperty* FillOutScoreParam = CastField<FIntProperty>(FindParameterForTest(StaticFillPayload, TEXT("OutScore")));
		ASSERT_THAT(IsNotNull(FillOutPayloadParam, TEXT("StaticFillPayload should reflect AS struct out parameter")));
		ASSERT_THAT(IsNotNull(FillOutScoreParam, TEXT("StaticFillPayload should reflect int out parameter")));
		if (FillOutPayloadParam == nullptr || FillOutScoreParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(ScorePayloadParam->Struct, FillOutPayloadParam->Struct,
			TEXT("global input and out payload parameters should share the generated struct type")));
		ASSERT_THAT(IsTrue(FillOutPayloadParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("global AS struct &out parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(FillOutPayloadParam->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReturnParm),
			TEXT("global AS struct &out parameter should not be const or return")));
		ASSERT_THAT(IsTrue(FillOutScoreParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("global int &out parameter should carry CPF_OutParm")));

		FStructProperty* ReturnPayloadParam = CastField<FStructProperty>(StaticReturnPayload->GetReturnProperty());
		FObjectProperty* ReturnWorldParam = CastField<FObjectProperty>(FindParameterForTest(StaticReturnPayload, TEXT("WorldContextObject")));
		FIntProperty* ReturnValueParam = CastField<FIntProperty>(FindParameterForTest(StaticReturnPayload, TEXT("Value")));
		ASSERT_THAT(IsNotNull(ReturnPayloadParam, TEXT("StaticReturnPayload should reflect AS struct return")));
		ASSERT_THAT(IsNotNull(ReturnWorldParam, TEXT("StaticReturnPayload should reflect explicit world-context object")));
		ASSERT_THAT(IsNotNull(ReturnValueParam, TEXT("StaticReturnPayload should reflect int input")));
		if (ReturnPayloadParam == nullptr || ReturnWorldParam == nullptr || ReturnValueParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(ScorePayloadParam->Struct, ReturnPayloadParam->Struct,
			TEXT("global AS struct return should share the generated struct type")));
		ASSERT_THAT(IsTrue(ReturnPayloadParam->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("global AS struct return should carry CPF_ReturnParm")));

		UObject* StaticsDefaultObject = StaticsClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(StaticsDefaultObject, TEXT("complex global statics class should expose a default object")));
		if (StaticsDefaultObject == nullptr)
		{
			return;
		}

		FFunctionInvoker ScoreInvoker(*TestRunner, StaticsDefaultObject, TEXT("StaticScorePayload"));
		ASSERT_THAT(IsTrue(ScoreInvoker.IsValid(), TEXT("StaticScorePayload should be invokable")));
		if (!ScoreInvoker.IsValid())
		{
			return;
		}
		ScoreInvoker.AddParam<UObject*>(StaticsDefaultObject);
		FProperty* PayloadSlotProperty = nullptr;
		void* PayloadSlot = nullptr;
		ASSERT_THAT(IsTrue(ScoreInvoker.AddParamSlot(PayloadSlotProperty, PayloadSlot),
			TEXT("StaticScorePayload should expose payload parameter slot")));
		FStructProperty* PayloadSlotStructProperty = CastField<FStructProperty>(PayloadSlotProperty);
		ASSERT_THAT(IsNotNull(PayloadSlotStructProperty, TEXT("payload parameter slot should be FStructProperty")));
		if (PayloadSlot == nullptr || PayloadSlotStructProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(ScorePayloadParam->Struct, PayloadSlotStructProperty->Struct,
			TEXT("payload parameter slot should use the generated struct type")));
		ASSERT_THAT(IsTrue(WritePayloadStructValue(*TestRunner, *PayloadSlotStructProperty, PayloadSlot, 10, FString(TEXT("Payload")))));

		FProperty* ValuesSlotProperty = nullptr;
		void* ValuesSlot = nullptr;
		ASSERT_THAT(IsTrue(ScoreInvoker.AddParamSlot(ValuesSlotProperty, ValuesSlot),
			TEXT("StaticScorePayload should expose TArray<int> parameter slot")));
		FArrayProperty* ValuesSlotArrayProperty = CastField<FArrayProperty>(ValuesSlotProperty);
		ASSERT_THAT(IsNotNull(ValuesSlotArrayProperty, TEXT("Values parameter slot should be FArrayProperty")));
		if (ValuesSlot == nullptr || ValuesSlotArrayProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddIntArrayValue(*TestRunner, *ValuesSlotArrayProperty, ValuesSlot, 7)));
		ASSERT_THAT(IsTrue(AddIntArrayValue(*TestRunner, *ValuesSlotArrayProperty, ValuesSlot, 8)));
		ASSERT_THAT(IsTrue(AddIntArrayValue(*TestRunner, *ValuesSlotArrayProperty, ValuesSlot, 9)));
		ASSERT_THAT(AreEqual(42, ScoreInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("complex global UFUNCTION should score world context, AS struct, and array inputs")));

		FFunctionInvoker FillInvoker(*TestRunner, StaticsDefaultObject, TEXT("StaticFillPayload"));
		ASSERT_THAT(IsTrue(FillInvoker.IsValid(), TEXT("StaticFillPayload should be invokable")));
		if (!FillInvoker.IsValid())
		{
			return;
		}
		FillInvoker.AddParam<UObject*>(StaticsDefaultObject);
		FProperty* FillPayloadSlotProperty = nullptr;
		void* FillPayloadSlot = nullptr;
		FProperty* FillScoreSlotProperty = nullptr;
		void* FillScoreSlot = nullptr;
		ASSERT_THAT(IsTrue(FillInvoker.AddParamSlot(FillPayloadSlotProperty, FillPayloadSlot),
			TEXT("StaticFillPayload should expose payload out slot")));
		ASSERT_THAT(IsTrue(FillInvoker.AddParamSlot(FillScoreSlotProperty, FillScoreSlot),
			TEXT("StaticFillPayload should expose score out slot")));
		FStructProperty* FillPayloadSlotStructProperty = CastField<FStructProperty>(FillPayloadSlotProperty);
		FIntProperty* FillScoreSlotIntProperty = CastField<FIntProperty>(FillScoreSlotProperty);
		ASSERT_THAT(IsNotNull(FillPayloadSlotStructProperty, TEXT("payload out slot should be FStructProperty")));
		ASSERT_THAT(IsNotNull(FillScoreSlotIntProperty, TEXT("score out slot should be FIntProperty")));
		if (FillPayloadSlot == nullptr || FillScoreSlot == nullptr || FillPayloadSlotStructProperty == nullptr || FillScoreSlotIntProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(FillInvoker.Call(), TEXT("StaticFillPayload should write reflected out parameters")));
		ASSERT_THAT(IsTrue(VerifyPayloadStructValue(*TestRunner, *FillPayloadSlotStructProperty, FillPayloadSlot, 17, FString(TEXT("GlobalPayload")),
			TEXT("StaticFillPayload out payload"))));
		ASSERT_THAT(AreEqual(30, FillScoreSlotIntProperty->GetPropertyValue(FillScoreSlot),
			TEXT("StaticFillPayload should write score out parameter")));

		FFunctionInvoker ReturnInvoker(*TestRunner, StaticsDefaultObject, TEXT("StaticReturnPayload"));
		ASSERT_THAT(IsTrue(ReturnInvoker.IsValid(), TEXT("StaticReturnPayload should be invokable")));
		if (!ReturnInvoker.IsValid())
		{
			return;
		}
		ReturnInvoker.AddParam<UObject*>(StaticsDefaultObject);
		ReturnInvoker.AddParam<int32>(42);
		ASSERT_THAT(IsTrue(ReturnInvoker.Call(), TEXT("StaticReturnPayload should execute")));
		void* ReturnSlot = ReturnPayloadParam->ContainerPtrToValuePtr<void>(ReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("StaticReturnPayload return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VerifyPayloadStructValue(*TestRunner, *ReturnPayloadParam, ReturnSlot, 42, FString(TEXT("ReturnedGlobal")),
			TEXT("StaticReturnPayload return payload"))));
	}

	TEST_METHOD(StaticGlobalAdvancedMetadataAndNamespaceMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_StaticAdvancedMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			namespace CoverageUFunctionStaticHelpers
			{
				int Scale(int Value)
				{
					return Value * 2;
				}

				int Mix(int A, int B)
				{
					return A + B + 3;
				}
			}

			UFUNCTION(BlueprintCallable, Category="Coverage|StaticMeta", meta=(DisplayName="Static Meta Action", Keywords="static meta coverage", ToolTip="Static meta tooltip", ShortToolTip="Static short tooltip", CompactNodeTitle="SMA", DeprecatedFunction, DeprecationMessage="Use StaticMetaReplacement", DevelopmentOnly, BlueprintInternalUseOnly, CustomCoverageKey="StaticCustom"))
			int StaticMetadataAction(int Value)
			{
				return CoverageUFunctionStaticHelpers::Scale(Value) + 2;
			}

			UFUNCTION(BlueprintPure, Category="Coverage|StaticMeta", meta=(ScriptName="StaticAliasName", ReturnDisplayName="Coverage Return", AdvancedDisplay="Bias"))
			int StaticPureAlias(int Value, int Bias = 1)
			{
				return CoverageUFunctionStaticHelpers::Mix(Value, Bias);
			}

			UFUNCTION(BlueprintCallable, Category="Coverage|StaticMeta")
			void StaticUPARAMAction(
				UPARAM(DisplayName="Input Value") int Input,
				UPARAM(DisplayName="Output Value", ref) int&out Output)
			{
				Output = CoverageUFunctionStaticHelpers::Scale(Input) + 10;
			}
			)AS");
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionStaticAdvancedMetadata.as"),
			ScriptSource);
		ASSERT_THAT(IsTrue(bCompiled, TEXT("static advanced metadata UFUNCTION module should compile")));
		if (!bCompiled)
		{
			return;
		}

		UClass* StaticsClass = FindGeneratedClass(&Engine, TEXT("UModule_ASCoverageUFunction_StaticAdvancedMetadataStatics"));
		ASSERT_THAT(IsNotNull(StaticsClass, TEXT("static advanced metadata functions should generate a statics class")));
		if (StaticsClass == nullptr)
		{
			return;
		}

		UFunction* StaticMetadataAction = FindFunctionForTest(StaticsClass, TEXT("StaticMetadataAction"));
		UFunction* StaticPureAlias = FindFunctionForTest(StaticsClass, TEXT("StaticPureAlias"));
		UFunction* StaticUPARAMAction = FindFunctionForTest(StaticsClass, TEXT("StaticUPARAMAction"));
		ASSERT_THAT(IsNotNull(StaticMetadataAction, TEXT("StaticMetadataAction should be generated")));
		ASSERT_THAT(IsNotNull(StaticPureAlias, TEXT("StaticPureAlias should be generated")));
		ASSERT_THAT(IsNotNull(StaticUPARAMAction, TEXT("StaticUPARAMAction should be generated")));
		if (StaticMetadataAction == nullptr || StaticPureAlias == nullptr || StaticUPARAMAction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(HasAllFunctionFlags(StaticMetadataAction, FUNC_Static | FUNC_BlueprintCallable),
			TEXT("static metadata action should be static and BlueprintCallable")));
		ASSERT_THAT(IsFalse(StaticMetadataAction->HasAnyFunctionFlags(FUNC_BlueprintPure),
			TEXT("static metadata action should not accidentally become pure")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|StaticMeta")), StaticMetadataAction->GetMetaData(TEXT("Category")),
			TEXT("static metadata action Category should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Static Meta Action")), StaticMetadataAction->GetMetaData(TEXT("DisplayName")),
			TEXT("static metadata action DisplayName should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("static meta coverage")), StaticMetadataAction->GetMetaData(TEXT("Keywords")),
			TEXT("static metadata action Keywords should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Static meta tooltip")), StaticMetadataAction->GetMetaData(TEXT("ToolTip")),
			TEXT("static metadata action ToolTip should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Static short tooltip")), StaticMetadataAction->GetMetaData(TEXT("ShortToolTip")),
			TEXT("static metadata action ShortToolTip should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("SMA")), StaticMetadataAction->GetMetaData(TEXT("CompactNodeTitle")),
			TEXT("static metadata action CompactNodeTitle should round-trip")));
		ASSERT_THAT(IsTrue(StaticMetadataAction->HasMetaData(TEXT("DeprecatedFunction")),
			TEXT("static metadata action DeprecatedFunction should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Use StaticMetaReplacement")), StaticMetadataAction->GetMetaData(TEXT("DeprecationMessage")),
			TEXT("static metadata action DeprecationMessage should round-trip")));
		ASSERT_THAT(IsTrue(StaticMetadataAction->HasMetaData(TEXT("DevelopmentOnly")),
			TEXT("static metadata action DevelopmentOnly should round-trip")));
		ASSERT_THAT(IsTrue(StaticMetadataAction->HasMetaData(TEXT("BlueprintInternalUseOnly")),
			TEXT("static metadata action BlueprintInternalUseOnly should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("StaticCustom")), StaticMetadataAction->GetMetaData(TEXT("CustomCoverageKey")),
			TEXT("static metadata action custom metadata should round-trip")));

		ASSERT_THAT(IsTrue(HasAllFunctionFlags(StaticPureAlias, FUNC_Static | FUNC_BlueprintCallable | FUNC_BlueprintPure),
			TEXT("static pure alias should be static, callable, and pure")));
		ASSERT_THAT(AreEqual(FString(TEXT("StaticAliasName")), StaticPureAlias->GetMetaData(TEXT("ScriptName")),
			TEXT("static pure alias ScriptName should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage Return")), StaticPureAlias->GetMetaData(TEXT("ReturnDisplayName")),
			TEXT("static pure alias ReturnDisplayName should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Bias")), StaticPureAlias->GetMetaData(TEXT("AdvancedDisplay")),
			TEXT("static pure alias AdvancedDisplay should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("1")), StaticPureAlias->GetMetaData(TEXT("CPP_Default_Bias")),
			TEXT("static pure alias default parameter should round-trip")));

		FIntProperty* StaticActionValue = CastField<FIntProperty>(FindParameterForTest(StaticMetadataAction, TEXT("Value")));
		FObjectProperty* StaticActionWorld = CastField<FObjectProperty>(FindParameterForTest(StaticMetadataAction, TEXT("_World_Context")));
		FIntProperty* StaticActionReturn = CastField<FIntProperty>(StaticMetadataAction->GetReturnProperty());
		FIntProperty* StaticPureValue = CastField<FIntProperty>(FindParameterForTest(StaticPureAlias, TEXT("Value")));
		FIntProperty* StaticPureBias = CastField<FIntProperty>(FindParameterForTest(StaticPureAlias, TEXT("Bias")));
		FObjectProperty* StaticPureWorld = CastField<FObjectProperty>(FindParameterForTest(StaticPureAlias, TEXT("_World_Context")));
		FIntProperty* StaticPureReturn = CastField<FIntProperty>(StaticPureAlias->GetReturnProperty());
		ASSERT_THAT(IsNotNull(StaticActionValue, TEXT("StaticMetadataAction Value parameter should reflect")));
		ASSERT_THAT(IsNotNull(StaticActionWorld, TEXT("StaticMetadataAction should reflect generated world-context parameter")));
		ASSERT_THAT(IsNotNull(StaticActionReturn, TEXT("StaticMetadataAction return should reflect")));
		ASSERT_THAT(IsNotNull(StaticPureValue, TEXT("StaticPureAlias Value parameter should reflect")));
		ASSERT_THAT(IsNotNull(StaticPureBias, TEXT("StaticPureAlias Bias parameter should reflect")));
		ASSERT_THAT(IsNotNull(StaticPureWorld, TEXT("StaticPureAlias should reflect generated world-context parameter")));
		ASSERT_THAT(IsNotNull(StaticPureReturn, TEXT("StaticPureAlias return should reflect")));
		if (StaticActionValue == nullptr || StaticActionWorld == nullptr || StaticActionReturn == nullptr || StaticPureValue == nullptr
			|| StaticPureBias == nullptr || StaticPureWorld == nullptr || StaticPureReturn == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(UObject::StaticClass(), StaticActionWorld->PropertyClass,
			TEXT("StaticMetadataAction generated world-context parameter should be UObject")));
		ASSERT_THAT(AreEqual(UObject::StaticClass(), StaticPureWorld->PropertyClass,
			TEXT("StaticPureAlias generated world-context parameter should be UObject")));
		ASSERT_THAT(IsFalse(StaticPureValue->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("AdvancedDisplay should not mark StaticPureAlias Value")));
		ASSERT_THAT(IsTrue(StaticPureBias->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("AdvancedDisplay should mark StaticPureAlias Bias")));
		ASSERT_THAT(IsTrue(StaticPureReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("StaticPureAlias return should carry CPF_ReturnParm")));

		const TArray<FProperty*> UPARAMParams = GetOrderedParameters(StaticUPARAMAction);
		ASSERT_THAT(AreEqual(3, UPARAMParams.Num(), TEXT("StaticUPARAMAction should expose two declared parameters plus generated world context")));
		ASSERT_THAT(AreEqual(FString(TEXT("Input Value")), GetParameterDisplayName(UPARAMParams.IsValidIndex(0) ? UPARAMParams[0] : nullptr),
			TEXT("static UPARAM input DisplayName should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Output Value")), GetParameterDisplayName(UPARAMParams.IsValidIndex(1) ? UPARAMParams[1] : nullptr),
			TEXT("static UPARAM output DisplayName should round-trip")));
		ASSERT_THAT(IsTrue(UPARAMParams.IsValidIndex(1) && UPARAMParams[1]->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("static UPARAM(ref) output should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(UPARAMParams.IsValidIndex(1) && UPARAMParams[1]->HasAnyPropertyFlags(CPF_ReferenceParm),
			TEXT("static UPARAM(ref) output should remain out-only")));
		FObjectProperty* UPARAMWorld = CastField<FObjectProperty>(UPARAMParams.IsValidIndex(2) ? UPARAMParams[2] : nullptr);
		ASSERT_THAT(IsNotNull(UPARAMWorld, TEXT("StaticUPARAMAction should append generated UObject world context")));
		if (UPARAMWorld == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(FName(TEXT("_World_Context")), UPARAMWorld->GetFName(),
			TEXT("StaticUPARAMAction generated world-context parameter should keep the standard name")));
		ASSERT_THAT(AreEqual(UObject::StaticClass(), UPARAMWorld->PropertyClass,
			TEXT("StaticUPARAMAction generated world-context parameter should be UObject")));

		UObject* StaticsDefaultObject = StaticsClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(StaticsDefaultObject, TEXT("static advanced metadata statics class should expose CDO")));
		if (StaticsDefaultObject == nullptr)
		{
			return;
		}

		FFunctionInvoker MetadataInvoker(*TestRunner, StaticsDefaultObject, TEXT("StaticMetadataAction"));
		ASSERT_THAT(IsTrue(MetadataInvoker.IsValid(), TEXT("StaticMetadataAction should be invokable")));
		if (!MetadataInvoker.IsValid())
		{
			return;
		}
		MetadataInvoker.AddParam<int32>(20);
		MetadataInvoker.AddParam<UObject*>(StaticsDefaultObject);
		ASSERT_THAT(AreEqual(42, MetadataInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("StaticMetadataAction should execute namespace helper call through statics class")));

		FFunctionInvoker PureInvoker(*TestRunner, StaticsDefaultObject, TEXT("StaticPureAlias"));
		ASSERT_THAT(IsTrue(PureInvoker.IsValid(), TEXT("StaticPureAlias should be invokable")));
		if (!PureInvoker.IsValid())
		{
			return;
		}
		PureInvoker.AddParam<int32>(30);
		PureInvoker.AddParam<int32>(9);
		PureInvoker.AddParam<UObject*>(StaticsDefaultObject);
		ASSERT_THAT(AreEqual(42, PureInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("StaticPureAlias should execute namespace helper with explicit default override")));

		FFunctionInvoker UPARAMInvoker(*TestRunner, StaticsDefaultObject, TEXT("StaticUPARAMAction"));
		ASSERT_THAT(IsTrue(UPARAMInvoker.IsValid(), TEXT("StaticUPARAMAction should be invokable")));
		if (!UPARAMInvoker.IsValid())
		{
			return;
		}
		UPARAMInvoker.AddParam<int32>(16);
		FProperty* OutputSlotProperty = nullptr;
		void* OutputSlot = nullptr;
		ASSERT_THAT(IsTrue(UPARAMInvoker.AddParamSlot(OutputSlotProperty, OutputSlot),
			TEXT("StaticUPARAMAction should expose out parameter slot")));
		FIntProperty* OutputIntProperty = CastField<FIntProperty>(OutputSlotProperty);
		ASSERT_THAT(IsNotNull(OutputIntProperty, TEXT("StaticUPARAMAction out slot should be FIntProperty")));
		if (OutputSlot == nullptr || OutputIntProperty == nullptr)
		{
			return;
		}
		UPARAMInvoker.AddParam<UObject*>(StaticsDefaultObject);
		ASSERT_THAT(IsTrue(UPARAMInvoker.Call(), TEXT("StaticUPARAMAction should write reflected out parameter")));
		ASSERT_THAT(AreEqual(42, OutputIntProperty->GetPropertyValue(OutputSlot),
			TEXT("StaticUPARAMAction should write namespace-computed out value")));
	}

	TEST_METHOD(NetworkSpecifierFlagMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_NetworkSpecifierFlags"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionNetworkActor : AActor
			{
				default SetReplicates(true);

				UFUNCTION(Server)
				void ServerReliableDefault()
				{
				}

				UFUNCTION(Server, Unreliable)
				void ServerUnreliableExplicit()
				{
				}

				UFUNCTION(Client)
				void ClientReliableDefault()
				{
				}

				UFUNCTION(Client, Unreliable)
				void ClientUnreliableExplicit()
				{
				}

				UFUNCTION(NetMulticast)
				void MulticastReliableExplicit()
				{
				}

				UFUNCTION(NetMulticast, Unreliable)
				void MulticastUnreliableExplicit()
				{
				}

				UFUNCTION(Server, WithValidation)
				void ServerValidatedReliable(int Value)
				{
				}

				UFUNCTION()
				bool ServerValidatedReliable_Validate(int Value)
				{
					return Value >= 0;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionNetworkSpecifierFlags.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionNetworkActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("network UFUNCTION flag matrix actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		struct FExpectedNetworkFunction
		{
			const TCHAR* Name;
			EFunctionFlags EndpointFlag;
			bool bReliable;
			bool bValidated;
		};

		const FExpectedNetworkFunction ExpectedFunctions[] = {
			{ TEXT("ServerReliableDefault"), FUNC_NetServer, true, false },
			{ TEXT("ServerUnreliableExplicit"), FUNC_NetServer, false, false },
			{ TEXT("ClientReliableDefault"), FUNC_NetClient, true, false },
			{ TEXT("ClientUnreliableExplicit"), FUNC_NetClient, false, false },
			{ TEXT("MulticastReliableExplicit"), FUNC_NetMulticast, true, false },
			{ TEXT("MulticastUnreliableExplicit"), FUNC_NetMulticast, false, false },
			{ TEXT("ServerValidatedReliable"), FUNC_NetServer, true, true },
		};

		for (const FExpectedNetworkFunction& Expected : ExpectedFunctions)
		{
			UFunction* Function = FindFunctionForTest(ScriptClass, Expected.Name);
			const FString GeneratedMessage = FString::Printf(TEXT("%s should be generated"), Expected.Name);
			ASSERT_THAT(IsNotNull(Function, *GeneratedMessage));
			if (Function == nullptr)
			{
				return;
			}

			const FString NetMessage = FString::Printf(TEXT("%s should carry FUNC_Net"), Expected.Name);
			ASSERT_THAT(IsTrue(Function->HasAnyFunctionFlags(FUNC_Net),
				*NetMessage));
			const FString EndpointMessage = FString::Printf(TEXT("%s should carry the declared endpoint flag"), Expected.Name);
			ASSERT_THAT(IsTrue(Function->HasAnyFunctionFlags(Expected.EndpointFlag),
				*EndpointMessage));
			const FString ReliabilityMessage = FString::Printf(TEXT("%s reliability flag should match declaration"), Expected.Name);
			ASSERT_THAT(AreEqual(Expected.bReliable, Function->HasAnyFunctionFlags(FUNC_NetReliable),
				*ReliabilityMessage));
			const FString ValidationMessage = FString::Printf(TEXT("%s validation flag should match declaration"), Expected.Name);
			ASSERT_THAT(AreEqual(Expected.bValidated, Function->HasAnyFunctionFlags(FUNC_NetValidate),
				*ValidationMessage));
		}

		UFunction* ValidateFunction = FindFunctionForTest(ScriptClass, TEXT("ServerValidatedReliable_Validate"));
		ASSERT_THAT(IsNotNull(ValidateFunction, TEXT("WithValidation companion should be generated")));
		if (ValidateFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(ValidateFunction->HasAnyFunctionFlags(FUNC_Net),
			TEXT("validation companion should not be routed as an RPC")));
		ASSERT_THAT(IsFalse(ValidateFunction->HasAnyFunctionFlags(FUNC_NetValidate),
			TEXT("validation companion should not itself carry FUNC_NetValidate")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(FindParameterForTest(FindFunctionForTest(ScriptClass, TEXT("ServerValidatedReliable")), TEXT("Value"))),
			TEXT("validated RPC should preserve its ordinary input parameter")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(FindParameterForTest(ValidateFunction, TEXT("Value"))),
			TEXT("validation companion should preserve the same input parameter")));
		ASSERT_THAT(IsNotNull(CastField<FBoolProperty>(FindParameterForTest(ValidateFunction, TEXT("ReturnValue"))),
			TEXT("validation companion should return bool")));
	}

	TEST_METHOD(NetworkSpecifierCallableAuthorityAndMetadataMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_NetworkCallableAuthorityMeta"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionNetworkMetaActor : AActor
			{
				default SetReplicates(true);

				UFUNCTION(Server, BlueprintAuthorityOnly, Category="Coverage|NetworkMeta", meta=(DisplayName="Authority Server Action", AdvancedDisplay="Reason"))
				void ServerAuthorityAction(int Value, FString Reason)
				{
				}

				UFUNCTION(Client, NotBlueprintCallable, CallInEditor, Category="Coverage|NetworkMeta")
				void ClientHiddenEditorNotify(int Value)
				{
				}

				UFUNCTION(NetMulticast, BlueprintCallable, Unreliable, Category="Coverage|NetworkMeta", meta=(Keywords="multicast unreliable"))
				void MulticastCallableUnreliable(int Value)
				{
				}

				UFUNCTION(Client, BlueprintCallable, WithValidation, Category="Coverage|NetworkMeta")
				void ClientValidatedNotify(int Value, FString Label)
				{
				}

				UFUNCTION()
				bool ClientValidatedNotify_Validate(int Value, FString Label)
				{
					return Value >= 0 && !Label.IsEmpty();
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionNetworkCallableAuthorityMeta.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionNetworkMetaActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("network callable/authority metadata actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* ServerAuthorityAction = FindFunctionForTest(ScriptClass, TEXT("ServerAuthorityAction"));
		UFunction* ClientHiddenEditorNotify = FindFunctionForTest(ScriptClass, TEXT("ClientHiddenEditorNotify"));
		UFunction* MulticastCallableUnreliable = FindFunctionForTest(ScriptClass, TEXT("MulticastCallableUnreliable"));
		UFunction* ClientValidatedNotify = FindFunctionForTest(ScriptClass, TEXT("ClientValidatedNotify"));
		UFunction* ClientValidatedNotifyValidate = FindFunctionForTest(ScriptClass, TEXT("ClientValidatedNotify_Validate"));
		ASSERT_THAT(IsNotNull(ServerAuthorityAction, TEXT("ServerAuthorityAction should be generated")));
		ASSERT_THAT(IsNotNull(ClientHiddenEditorNotify, TEXT("ClientHiddenEditorNotify should be generated")));
		ASSERT_THAT(IsNotNull(MulticastCallableUnreliable, TEXT("MulticastCallableUnreliable should be generated")));
		ASSERT_THAT(IsNotNull(ClientValidatedNotify, TEXT("ClientValidatedNotify should be generated")));
		ASSERT_THAT(IsNotNull(ClientValidatedNotifyValidate, TEXT("ClientValidatedNotify_Validate should be generated")));
		if (ServerAuthorityAction == nullptr || ClientHiddenEditorNotify == nullptr || MulticastCallableUnreliable == nullptr
			|| ClientValidatedNotify == nullptr || ClientValidatedNotifyValidate == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(HasAllFunctionFlags(ServerAuthorityAction,
			FUNC_Net | FUNC_NetServer | FUNC_NetReliable | FUNC_BlueprintCallable | FUNC_BlueprintAuthorityOnly),
			TEXT("server authority RPC should carry net, callable, reliable, and authority flags")));
		ASSERT_THAT(IsFalse(ServerAuthorityAction->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("plain server RPC should use generated event wrapper without becoming BlueprintEvent-overridable")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|NetworkMeta")), ServerAuthorityAction->GetMetaData(TEXT("Category")),
			TEXT("server authority RPC Category should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Authority Server Action")), ServerAuthorityAction->GetMetaData(TEXT("DisplayName")),
			TEXT("server authority RPC DisplayName should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Reason")), ServerAuthorityAction->GetMetaData(TEXT("AdvancedDisplay")),
			TEXT("server authority RPC AdvancedDisplay should round-trip")));

		FIntProperty* ServerValue = CastField<FIntProperty>(FindParameterForTest(ServerAuthorityAction, TEXT("Value")));
		FStrProperty* ServerReason = CastField<FStrProperty>(FindParameterForTest(ServerAuthorityAction, TEXT("Reason")));
		ASSERT_THAT(IsNotNull(ServerValue, TEXT("server authority RPC int parameter should reflect")));
		ASSERT_THAT(IsNotNull(ServerReason, TEXT("server authority RPC FString parameter should reflect")));
		if (ServerValue == nullptr || ServerReason == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(ServerValue->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("AdvancedDisplay should not mark the required server RPC value")));
		ASSERT_THAT(IsTrue(ServerReason->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("AdvancedDisplay should mark the optional server RPC reason")));

		ASSERT_THAT(IsTrue(HasAllFunctionFlags(ClientHiddenEditorNotify,
			FUNC_Net | FUNC_NetClient | FUNC_NetReliable),
			TEXT("hidden client RPC should carry net/client/reliable flags")));
		ASSERT_THAT(IsFalse(ClientHiddenEditorNotify->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("hidden client RPC should use generated event wrapper without becoming BlueprintEvent-overridable")));
		ASSERT_THAT(IsFalse(ClientHiddenEditorNotify->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("NotBlueprintCallable should suppress client RPC BlueprintCallable visibility")));
		ASSERT_THAT(IsTrue(ClientHiddenEditorNotify->HasMetaData(TEXT("CallInEditor")),
			TEXT("CallInEditor metadata should survive on hidden client RPC")));

		ASSERT_THAT(IsTrue(HasAllFunctionFlags(MulticastCallableUnreliable,
			FUNC_Net | FUNC_NetMulticast | FUNC_BlueprintCallable),
			TEXT("unreliable multicast RPC should carry net/multicast/callable flags")));
		ASSERT_THAT(IsFalse(MulticastCallableUnreliable->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("unreliable multicast RPC should use generated event wrapper without becoming BlueprintEvent-overridable")));
		ASSERT_THAT(IsFalse(MulticastCallableUnreliable->HasAnyFunctionFlags(FUNC_NetReliable),
			TEXT("Unreliable multicast RPC should not carry FUNC_NetReliable")));
		ASSERT_THAT(AreEqual(FString(TEXT("multicast unreliable")), MulticastCallableUnreliable->GetMetaData(TEXT("Keywords")),
			TEXT("multicast RPC Keywords metadata should round-trip")));

		ASSERT_THAT(IsTrue(HasAllFunctionFlags(ClientValidatedNotify,
			FUNC_Net | FUNC_NetClient | FUNC_NetReliable | FUNC_NetValidate | FUNC_BlueprintCallable),
			TEXT("client validated RPC should carry net/client/reliable/validate/callable flags")));
		ASSERT_THAT(IsFalse(ClientValidatedNotify->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("client validated RPC should use generated event wrapper without becoming BlueprintEvent-overridable")));
		ASSERT_THAT(IsFalse(ClientValidatedNotifyValidate->HasAnyFunctionFlags(FUNC_Net),
			TEXT("client validation companion should not be routed as RPC")));
		ASSERT_THAT(IsFalse(ClientValidatedNotifyValidate->HasAnyFunctionFlags(FUNC_NetValidate),
			TEXT("client validation companion should not carry FUNC_NetValidate")));

		FIntProperty* ClientValue = CastField<FIntProperty>(FindParameterForTest(ClientValidatedNotify, TEXT("Value")));
		FStrProperty* ClientLabel = CastField<FStrProperty>(FindParameterForTest(ClientValidatedNotify, TEXT("Label")));
		FIntProperty* ValidateValue = CastField<FIntProperty>(FindParameterForTest(ClientValidatedNotifyValidate, TEXT("Value")));
		FStrProperty* ValidateLabel = CastField<FStrProperty>(FindParameterForTest(ClientValidatedNotifyValidate, TEXT("Label")));
		FBoolProperty* ValidateReturn = CastField<FBoolProperty>(ClientValidatedNotifyValidate->GetReturnProperty());
		ASSERT_THAT(IsNotNull(ClientValue, TEXT("client validated RPC Value parameter should reflect")));
		ASSERT_THAT(IsNotNull(ClientLabel, TEXT("client validated RPC Label parameter should reflect")));
		ASSERT_THAT(IsNotNull(ValidateValue, TEXT("client validate companion Value parameter should reflect")));
		ASSERT_THAT(IsNotNull(ValidateLabel, TEXT("client validate companion Label parameter should reflect")));
		ASSERT_THAT(IsNotNull(ValidateReturn, TEXT("client validate companion bool return should reflect")));
		if (ClientValue == nullptr || ClientLabel == nullptr || ValidateValue == nullptr || ValidateLabel == nullptr || ValidateReturn == nullptr)
		{
			return;
		}

		TArray<FProperty*> EndpointParams = GetOrderedParameters(ClientValidatedNotify);
		TArray<FProperty*> ValidateParams = GetOrderedParameters(ClientValidatedNotifyValidate);
		ASSERT_THAT(AreEqual(2, EndpointParams.Num(), TEXT("client validated RPC should expose two endpoint parameters")));
		ASSERT_THAT(AreEqual(2, ValidateParams.Num(), TEXT("client validate companion should expose two parameters")));
		if (EndpointParams.Num() != 2 || ValidateParams.Num() != 2)
		{
			return;
		}
		ASSERT_THAT(AreEqual(FName(TEXT("Value")), EndpointParams[0]->GetFName(),
			TEXT("client validated RPC should keep Value as first parameter")));
		ASSERT_THAT(AreEqual(FName(TEXT("Label")), EndpointParams[1]->GetFName(),
			TEXT("client validated RPC should keep Label as second parameter")));
		ASSERT_THAT(AreEqual(FName(TEXT("Value")), ValidateParams[0]->GetFName(),
			TEXT("client validate companion should keep Value as first parameter")));
		ASSERT_THAT(AreEqual(FName(TEXT("Label")), ValidateParams[1]->GetFName(),
			TEXT("client validate companion should keep Label as second parameter")));
		ASSERT_THAT(IsTrue(ValidateReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("client validate companion bool should be the return parameter")));
	}

	TEST_METHOD(WorldContextMetadataReflectsParameterName)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_WorldContextMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionWorldContextMetadata.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionWorldContextActor : AActor
			{
				UFUNCTION(BlueprintCallable, Category="Coverage|WorldContext", meta=(WorldContext="WorldContextObject"))
				int ReadWithWorldContext(UObject WorldContextObject, int Value)
				{
					return Value;
				}
			}
			)AS"),
			TEXT("ACoverageUFunctionWorldContextActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("WorldContext metadata actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* ReadWithWorldContext = FindFunctionForTest(ScriptClass, TEXT("ReadWithWorldContext"));
		ASSERT_THAT(IsNotNull(ReadWithWorldContext, TEXT("WorldContext UFUNCTION should be generated")));
		if (ReadWithWorldContext == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("WorldContextObject")), ReadWithWorldContext->GetMetaData(TEXT("WorldContext")),
			TEXT("WorldContext metadata should preserve the context parameter name")));

		FProperty* WorldContextParam = FindParameterForTest(ReadWithWorldContext, TEXT("WorldContextObject"));
		ASSERT_THAT(IsNotNull(WorldContextParam, TEXT("WorldContextObject parameter should be reflected")));
		if (WorldContextParam == nullptr)
		{
			return;
		}

		FObjectProperty* WorldContextObjectProperty = CastField<FObjectProperty>(WorldContextParam);
		ASSERT_THAT(IsNotNull(WorldContextObjectProperty, TEXT("WorldContextObject should reflect as an object property")));
		if (WorldContextObjectProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(UObject::StaticClass(), WorldContextObjectProperty->PropertyClass,
			TEXT("WorldContextObject should preserve the UObject reflected type")));

		FProperty* ReturnValue = FindParameterForTest(ReadWithWorldContext, TEXT("ReturnValue"));
		ASSERT_THAT(IsNotNull(ReturnValue, TEXT("WorldContext UFUNCTION return value should be reflected")));
		if (ReturnValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ReturnValue->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("WorldContext UFUNCTION return value should carry CPF_ReturnParm")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(ReturnValue),
			TEXT("WorldContext UFUNCTION return value should reflect as FIntProperty")));
	}

	TEST_METHOD(BasicMemberAndConstReflectionCall)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_BasicMemberConst"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionBasicMemberConst.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionBasicActor : AActor
			{
				UPROPERTY()
				int StoredValue = 5;

				UFUNCTION()
				void SetStoredValue(int Value)
				{
					StoredValue = Value;
				}

				UFUNCTION()
				int GetStoredValue() const
				{
					return StoredValue;
				}
			}
			)AS"),
			TEXT("ACoverageUFunctionBasicActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Basic UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* SetStoredValue = FindFunctionForTest(ScriptClass, TEXT("SetStoredValue"));
		UFunction* GetStoredValue = FindFunctionForTest(ScriptClass, TEXT("GetStoredValue"));
		ASSERT_THAT(IsNotNull(SetStoredValue, TEXT("UFUNCTION() class member method should be generated")));
		ASSERT_THAT(IsNotNull(GetStoredValue, TEXT("UFUNCTION() const class member method should be generated")));
		if (SetStoredValue == nullptr || GetStoredValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(SetStoredValue->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("Plain UFUNCTION member should be BlueprintCallable through reflection")));
		ASSERT_THAT(IsFalse(SetStoredValue->HasAnyFunctionFlags(FUNC_Const),
			TEXT("Non-const UFUNCTION member should not set FUNC_Const")));
		ASSERT_THAT(IsTrue(GetStoredValue->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("Plain const UFUNCTION member should be BlueprintCallable through reflection")));
		ASSERT_THAT(IsTrue(GetStoredValue->HasAnyFunctionFlags(FUNC_Const),
			TEXT("const UFUNCTION member should set FUNC_Const")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Basic UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker SetInvoker(*TestRunner, Actor, TEXT("SetStoredValue"));
		ASSERT_THAT(IsTrue(SetInvoker.IsValid(), TEXT("SetStoredValue should be invokable through reflection")));
		if (!SetInvoker.IsValid())
		{
			return;
		}
		SetInvoker.AddParam<int32>(123);
		ASSERT_THAT(IsTrue(SetInvoker.Call(), TEXT("SetStoredValue reflected invocation should succeed")));

		FFunctionInvoker GetInvoker(*TestRunner, Actor, TEXT("GetStoredValue"));
		ASSERT_THAT(IsTrue(GetInvoker.IsValid(), TEXT("GetStoredValue should be invokable through reflection")));
		if (!GetInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(123, GetInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("const UFUNCTION reflected invocation should return current object state")));
	}

	TEST_METHOD(BlueprintOverrideBeginPlayTickAndSuperExecute)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_BlueprintOverrideLifecycle"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionBlueprintOverrideLifecycle.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionOverrideBase : AActor
			{
				UPROPERTY()
				int BeginPlayCount = 0;

				UPROPERTY()
				int TickCount = 0;

				UPROPERTY()
				int LastDeltaMillis = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BeginPlayCount += 1;
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaSeconds)
				{
					TickCount += 1;
				}
			}

			UCLASS()
			class ACoverageUFunctionOverrideChild : ACoverageUFunctionOverrideBase
			{
				UPROPERTY()
				int ChildBeginPlayCount = 0;

				UPROPERTY()
				int ChildTickCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Super::BeginPlay();
					ChildBeginPlayCount += 1;
				}

				UFUNCTION(BlueprintOverride)
				void Tick(float DeltaSeconds)
				{
					Super::Tick(DeltaSeconds);
					ChildTickCount += 1;
					LastDeltaMillis = int(DeltaSeconds * 1000.0f);
				}
			}
			)AS"),
			TEXT("ACoverageUFunctionOverrideBase"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("BlueprintOverride lifecycle base actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UClass* ChildClass = FindGeneratedClass(&Engine, TEXT("ACoverageUFunctionOverrideChild"));
		ASSERT_THAT(IsNotNull(ChildClass, TEXT("BlueprintOverride lifecycle child actor should be generated")));
		if (ChildClass == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ChildClass->IsChildOf(ScriptClass), TEXT("BlueprintOverride lifecycle child should inherit from base actor")));

		UFunction* BeginPlayFunction = FindFunctionForTest(ChildClass, TEXT("BeginPlay"));
		UFunction* TickFunction = FindFunctionForTest(ChildClass, TEXT("Tick"));
		ASSERT_THAT(IsNotNull(BeginPlayFunction, TEXT("BeginPlay BlueprintOverride should be generated")));
		ASSERT_THAT(IsNotNull(TickFunction, TEXT("Tick BlueprintOverride should be generated")));
		if (BeginPlayFunction == nullptr || TickFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(BeginPlayFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("BlueprintOverride BeginPlay should surface as a BlueprintEvent function")));
		ASSERT_THAT(IsTrue(TickFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("BlueprintOverride Tick should surface as a BlueprintEvent function")));
		ASSERT_THAT(IsNotNull(FindParameterForTest(TickFunction, TEXT("DeltaSeconds")),
			TEXT("Tick override should expose DeltaSeconds parameter")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ChildClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("BlueprintOverride lifecycle actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		{
			FAngelscriptEngineScope ActorScope(Engine, Actor);
			Actor->Tick(0.025f);
			Actor->Tick(0.025f);
		}

		int32 BeginPlayCount = 0;
		int32 TickCount = 0;
		int32 ChildBeginPlayCount = 0;
		int32 ChildTickCount = 0;
		int32 LastDeltaMillis = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, Actor, TEXT("BeginPlayCount"), BeginPlayCount),
			TEXT("BeginPlayCount property should be readable")));
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, Actor, TEXT("TickCount"), TickCount),
			TEXT("TickCount property should be readable")));
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, Actor, TEXT("ChildBeginPlayCount"), ChildBeginPlayCount),
			TEXT("ChildBeginPlayCount property should be readable")));
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, Actor, TEXT("ChildTickCount"), ChildTickCount),
			TEXT("ChildTickCount property should be readable")));
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, Actor, TEXT("LastDeltaMillis"), LastDeltaMillis),
			TEXT("LastDeltaMillis property should be readable")));
		ASSERT_THAT(AreEqual(1, BeginPlayCount, TEXT("Super::BeginPlay should execute the parent BlueprintOverride once")));
		ASSERT_THAT(AreEqual(2, TickCount, TEXT("Super::Tick should execute the parent BlueprintOverride through actor tick dispatch")));
		ASSERT_THAT(AreEqual(1, ChildBeginPlayCount, TEXT("Child BeginPlay BlueprintOverride should execute once")));
		ASSERT_THAT(AreEqual(2, ChildTickCount, TEXT("Child Tick BlueprintOverride should execute through actor tick dispatch")));
		ASSERT_THAT(AreEqual(25, LastDeltaMillis, TEXT("Child Tick BlueprintOverride should receive DeltaSeconds")));
	}

	TEST_METHOD(BlueprintOverrideNativeActorEventParameterMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_NativeActorEventParameters"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionNativeEventActor : AActor
			{
				UPROPERTY()
				int ConstructionCount = 0;

				UPROPERTY()
				int DestroyedCount = 0;

				UPROPERTY()
				int EndPlayCount = 0;

				UPROPERTY()
				int ResetCount = 0;

				UPROPERTY()
				int LastReason = -1;

				UPROPERTY()
				int LastTransformScore = 0;

				UFUNCTION(BlueprintOverride)
				void UserConstructionScript()
				{
					ConstructionCount += 1;
				}

				UFUNCTION(BlueprintOverride)
				void ActorBeginOverlap(AActor OtherActor)
				{
					LastReason = OtherActor == this ? 11 : -11;
				}

				UFUNCTION(BlueprintOverride)
				void ActorEndOverlap(AActor OtherActor)
				{
					LastReason = OtherActor == this ? 22 : -22;
				}

				UFUNCTION(BlueprintOverride)
				void EndPlay(EEndPlayReason EndPlayReason)
				{
					EndPlayCount += 1;
					LastReason = int(EndPlayReason);
				}

				UFUNCTION(BlueprintOverride)
				void Destroyed()
				{
					DestroyedCount += 1;
				}

				UFUNCTION(BlueprintOverride)
				void OnReset()
				{
					ResetCount += 1;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|NativeEvents")
				void DispatchActorEvents()
				{
					ActorBeginOverlap(this);
					ActorEndOverlap(this);
					EndPlay(EEndPlayReason::Destroyed);
					Destroyed();
					OnReset();
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|NativeEvents")
				void ReadTransformByConstRef(const FTransform&in Transform)
				{
					FVector Location = Transform.Location;
					LastTransformScore = int(Location.X + Location.Y + Location.Z);
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionNativeActorEventParameters.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionNativeEventActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("native actor event override actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* UserConstructionScript = FindFunctionForTest(ScriptClass, TEXT("UserConstructionScript"));
		UFunction* ActorBeginOverlap = FindFunctionForTest(ScriptClass, TEXT("ActorBeginOverlap"));
		UFunction* ActorEndOverlap = FindFunctionForTest(ScriptClass, TEXT("ActorEndOverlap"));
		UFunction* EndPlay = FindFunctionForTest(ScriptClass, TEXT("EndPlay"));
		UFunction* Destroyed = FindFunctionForTest(ScriptClass, TEXT("Destroyed"));
		UFunction* OnReset = FindFunctionForTest(ScriptClass, TEXT("OnReset"));
		UFunction* DispatchActorEvents = FindFunctionForTest(ScriptClass, TEXT("DispatchActorEvents"));
		UFunction* ReadTransformByConstRef = FindFunctionForTest(ScriptClass, TEXT("ReadTransformByConstRef"));
		ASSERT_THAT(IsNotNull(UserConstructionScript, TEXT("UserConstructionScript BlueprintOverride should be generated")));
		ASSERT_THAT(IsNotNull(ActorBeginOverlap, TEXT("ActorBeginOverlap BlueprintOverride should be generated")));
		ASSERT_THAT(IsNotNull(ActorEndOverlap, TEXT("ActorEndOverlap BlueprintOverride should be generated")));
		ASSERT_THAT(IsNotNull(EndPlay, TEXT("EndPlay BlueprintOverride should be generated")));
		ASSERT_THAT(IsNotNull(Destroyed, TEXT("Destroyed BlueprintOverride should be generated")));
		ASSERT_THAT(IsNotNull(OnReset, TEXT("OnReset BlueprintOverride should be generated")));
		ASSERT_THAT(IsNotNull(DispatchActorEvents, TEXT("DispatchActorEvents helper should be generated")));
		ASSERT_THAT(IsNotNull(ReadTransformByConstRef, TEXT("ReadTransformByConstRef helper should be generated")));
		if (UserConstructionScript == nullptr || ActorBeginOverlap == nullptr || ActorEndOverlap == nullptr || EndPlay == nullptr
			|| Destroyed == nullptr || OnReset == nullptr || DispatchActorEvents == nullptr || ReadTransformByConstRef == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(UserConstructionScript->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("UserConstructionScript override should surface as BlueprintEvent")));
		ASSERT_THAT(IsTrue(ActorBeginOverlap->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("ActorBeginOverlap override should surface as BlueprintEvent")));
		ASSERT_THAT(IsTrue(ActorEndOverlap->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("ActorEndOverlap override should surface as BlueprintEvent")));
		ASSERT_THAT(IsTrue(EndPlay->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("EndPlay override should surface as BlueprintEvent")));
		ASSERT_THAT(IsTrue(Destroyed->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("Destroyed override should surface as BlueprintEvent")));
		ASSERT_THAT(IsTrue(OnReset->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("OnReset override should surface as BlueprintEvent")));

		FObjectProperty* BeginOverlapOtherActor = CastField<FObjectProperty>(FindParameterForTest(ActorBeginOverlap, TEXT("OtherActor")));
		FObjectProperty* EndOverlapOtherActor = CastField<FObjectProperty>(FindParameterForTest(ActorEndOverlap, TEXT("OtherActor")));
		FProperty* EndPlayReason = FindParameterForTest(EndPlay, TEXT("EndPlayReason"));
		FStructProperty* TransformParam = CastField<FStructProperty>(FindParameterForTest(ReadTransformByConstRef, TEXT("Transform")));
		ASSERT_THAT(IsNotNull(BeginOverlapOtherActor, TEXT("ActorBeginOverlap OtherActor should reflect as FObjectProperty")));
		ASSERT_THAT(IsNotNull(EndOverlapOtherActor, TEXT("ActorEndOverlap OtherActor should reflect as FObjectProperty")));
		ASSERT_THAT(IsNotNull(EndPlayReason, TEXT("EndPlay reason should reflect as an enum-compatible property")));
		ASSERT_THAT(IsNotNull(TransformParam, TEXT("const FTransform&in should reflect as FStructProperty")));
		if (BeginOverlapOtherActor == nullptr || EndOverlapOtherActor == nullptr || EndPlayReason == nullptr || TransformParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(EndPlayReason->IsA<FByteProperty>() || EndPlayReason->IsA<FEnumProperty>(),
			TEXT("EndPlay reason should reflect as FByteProperty or FEnumProperty")));

		ASSERT_THAT(AreEqual(AActor::StaticClass(), BeginOverlapOtherActor->PropertyClass,
			TEXT("ActorBeginOverlap OtherActor should preserve AActor type")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), EndOverlapOtherActor->PropertyClass,
			TEXT("ActorEndOverlap OtherActor should preserve AActor type")));
		ASSERT_THAT(AreEqual(TBaseStructure<FTransform>::Get(), TransformParam->Struct,
			TEXT("const FTransform&in should preserve native struct type")));
		ASSERT_THAT(IsTrue(TransformParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("const FTransform&in should carry reference out-parm layout")));
		ASSERT_THAT(IsTrue(TransformParam->HasAnyPropertyFlags(CPF_ConstParm),
			TEXT("const FTransform&in should carry CPF_ConstParm")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("native actor event override actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker DispatchInvoker(*TestRunner, Actor, TEXT("DispatchActorEvents"));
		ASSERT_THAT(IsTrue(DispatchInvoker.IsValid(), TEXT("DispatchActorEvents should be invokable")));
		if (!DispatchInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(DispatchInvoker.Call(), TEXT("DispatchActorEvents should execute all native event wrappers")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EndPlayCount"), 1,
			TEXT("EndPlay override should execute through script helper dispatch"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DestroyedCount"), 1,
			TEXT("Destroyed override should execute through script helper dispatch"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ResetCount"), 1,
			TEXT("Reset override should execute through script helper dispatch"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastReason"), static_cast<int32>(EEndPlayReason::Destroyed),
			TEXT("EndPlay enum parameter should cross the UFUNCTION wrapper"))));

		FFunctionInvoker TransformInvoker(*TestRunner, Actor, TEXT("ReadTransformByConstRef"));
		ASSERT_THAT(IsTrue(TransformInvoker.IsValid(), TEXT("ReadTransformByConstRef should be invokable")));
		if (!TransformInvoker.IsValid())
		{
			return;
		}
		TransformInvoker.AddParam<FTransform>(FTransform(FRotator::ZeroRotator, FVector(10.0, 20.0, 12.0), FVector::OneVector));
		ASSERT_THAT(IsTrue(TransformInvoker.Call(), TEXT("ReadTransformByConstRef should consume const-ref FTransform")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastTransformScore"), 42,
			TEXT("const-ref FTransform parameter should reach script code"))));
	}

	TEST_METHOD(BlueprintEventReflectsAndInvokesImplementation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_BlueprintEvent"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionBlueprintEvent.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionEventActor : AActor
			{
				UPROPERTY()
				int EventCallCount = 0;

				UPROPERTY()
				int LastInputValue = 0;

				UFUNCTION(BlueprintEvent, Category="Coverage|Events", meta=(DisplayName="Compute Event Value"))
				int ComputeEventValue(int Value)
				{
					EventCallCount += 1;
					LastInputValue = Value;
					return Value + 17;
				}

				UFUNCTION()
				int CallEventFromScript(int Value)
				{
					return ComputeEventValue(Value);
				}
			}
			)AS"),
			TEXT("ACoverageUFunctionEventActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("BlueprintEvent actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* EventFunction = FindFunctionForTest(ScriptClass, TEXT("ComputeEventValue"));
		UFunction* CallerFunction = FindFunctionForTest(ScriptClass, TEXT("CallEventFromScript"));
		ASSERT_THAT(IsNotNull(EventFunction, TEXT("BlueprintEvent function should be generated")));
		ASSERT_THAT(IsNotNull(CallerFunction, TEXT("BlueprintEvent caller should be generated")));
		if (EventFunction == nullptr || CallerFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(EventFunction->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("BlueprintEvent should set FUNC_BlueprintEvent")));
		ASSERT_THAT(IsFalse(EventFunction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("Plain BlueprintEvent should not imply BlueprintCallable")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Events")), EventFunction->GetMetaData(TEXT("Category")),
			TEXT("BlueprintEvent Category metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("Compute Event Value")), EventFunction->GetMetaData(TEXT("DisplayName")),
			TEXT("BlueprintEvent DisplayName metadata should be preserved")));
		ASSERT_THAT(IsNotNull(FindParameterForTest(EventFunction, TEXT("Value")),
			TEXT("BlueprintEvent should expose its input parameter")));
		ASSERT_THAT(IsNotNull(FindParameterForTest(EventFunction, TEXT("ReturnValue")),
			TEXT("BlueprintEvent should expose its return parameter")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("BlueprintEvent actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker EventInvoker(*TestRunner, Actor, TEXT("ComputeEventValue"));
		ASSERT_THAT(IsTrue(EventInvoker.IsValid(), TEXT("BlueprintEvent should be invokable through reflection")));
		if (!EventInvoker.IsValid())
		{
			return;
		}
		EventInvoker.AddParam<int32>(25);
		ASSERT_THAT(AreEqual(42, EventInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("BlueprintEvent reflected invocation should route to script implementation")));

		int32 EventCallCount = 0;
		int32 LastInputValue = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, Actor, TEXT("EventCallCount"), EventCallCount),
			TEXT("EventCallCount property should be readable after reflected invocation")));
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, Actor, TEXT("LastInputValue"), LastInputValue),
			TEXT("LastInputValue property should be readable after reflected invocation")));
		ASSERT_THAT(AreEqual(1, EventCallCount, TEXT("BlueprintEvent reflected invocation should execute once")));
		ASSERT_THAT(AreEqual(25, LastInputValue, TEXT("BlueprintEvent reflected invocation should pass the input value")));

		FFunctionInvoker CallerInvoker(*TestRunner, Actor, TEXT("CallEventFromScript"));
		ASSERT_THAT(IsTrue(CallerInvoker.IsValid(), TEXT("CallEventFromScript should be invokable through reflection")));
		if (!CallerInvoker.IsValid())
		{
			return;
		}
		CallerInvoker.AddParam<int32>(5);
		ASSERT_THAT(AreEqual(22, CallerInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Script call site should route through the generated BlueprintEvent wrapper")));
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, Actor, TEXT("EventCallCount"), EventCallCount),
			TEXT("EventCallCount property should be readable after script invocation")));
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, Actor, TEXT("LastInputValue"), LastInputValue),
			TEXT("LastInputValue property should be readable after script invocation")));
		ASSERT_THAT(AreEqual(2, EventCallCount, TEXT("BlueprintEvent script invocation should execute a second time")));
		ASSERT_THAT(AreEqual(5, LastInputValue, TEXT("BlueprintEvent script invocation should pass the input value")));
	}

	TEST_METHOD(BlueprintEventCallablePureAndOutParameterMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_BlueprintEventCombinationMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionEventCombinationActor : AActor
			{
				UPROPERTY()
				int PlainEventCalls = 0;

				UPROPERTY()
				int CallableEventCalls = 0;

				UPROPERTY()
				int OutEventCalls = 0;

				UFUNCTION(BlueprintEvent, Category="Coverage|EventCombos")
				void PlainEvent()
				{
					PlainEventCalls += 1;
				}

				UFUNCTION(BlueprintEvent, BlueprintCallable, Category="Coverage|EventCombos", meta=(DisplayName="Callable Event", Keywords="event callable"))
				int CallableEvent(int Value)
				{
					CallableEventCalls += 1;
					return Value + 10;
				}

				UFUNCTION(BlueprintEvent, BlueprintPure, Category="Coverage|EventCombos", meta=(CompactNodeTitle="PEV"))
				int PureConstEvent(int Value) const
				{
					return Value + 20;
				}

				UFUNCTION(BlueprintEvent, BlueprintCallable, Category="Coverage|EventCombos", meta=(AdvancedDisplay="Label"))
				int OutEvent(int Value, FString Label, int&out OutValue)
				{
					OutEventCalls += 1;
					OutValue = Value + Label.Len();
					return OutValue + 5;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|EventCombos")
				int DispatchEventCombinationMatrix()
				{
					PlainEvent();
					int Score = CallableEvent(12);
					Score += PureConstEvent(10);
					int OutValue = 0;
					Score += OutEvent(30, "Seven!!", OutValue);
					return Score + OutValue;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionBlueprintEventCombinationMatrix.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionEventCombinationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("BlueprintEvent combination actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* PlainEvent = FindFunctionForTest(ScriptClass, TEXT("PlainEvent"));
		UFunction* CallableEvent = FindFunctionForTest(ScriptClass, TEXT("CallableEvent"));
		UFunction* PureConstEvent = FindFunctionForTest(ScriptClass, TEXT("PureConstEvent"));
		UFunction* OutEvent = FindFunctionForTest(ScriptClass, TEXT("OutEvent"));
		UFunction* DispatchEventCombinationMatrix = FindFunctionForTest(ScriptClass, TEXT("DispatchEventCombinationMatrix"));
		ASSERT_THAT(IsNotNull(PlainEvent, TEXT("plain BlueprintEvent should be generated")));
		ASSERT_THAT(IsNotNull(CallableEvent, TEXT("callable BlueprintEvent should be generated")));
		ASSERT_THAT(IsNotNull(PureConstEvent, TEXT("pure const BlueprintEvent should be generated")));
		ASSERT_THAT(IsNotNull(OutEvent, TEXT("out-param BlueprintEvent should be generated")));
		ASSERT_THAT(IsNotNull(DispatchEventCombinationMatrix, TEXT("event combination dispatcher should be generated")));
		if (PlainEvent == nullptr || CallableEvent == nullptr || PureConstEvent == nullptr
			|| OutEvent == nullptr || DispatchEventCombinationMatrix == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(PlainEvent->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("plain BlueprintEvent should carry FUNC_BlueprintEvent")));
		ASSERT_THAT(IsFalse(PlainEvent->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure),
			TEXT("plain BlueprintEvent should not imply callable or pure flags")));

		ASSERT_THAT(IsTrue(HasAllFunctionFlags(CallableEvent, FUNC_BlueprintEvent | FUNC_BlueprintCallable),
			TEXT("BlueprintEvent plus BlueprintCallable should carry both flags")));
		ASSERT_THAT(IsFalse(CallableEvent->HasAnyFunctionFlags(FUNC_BlueprintPure | FUNC_Const),
			TEXT("callable event should not accidentally become pure or const")));
		ASSERT_THAT(AreEqual(FString(TEXT("Callable Event")), CallableEvent->GetMetaData(TEXT("DisplayName")),
			TEXT("callable event DisplayName metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("event callable")), CallableEvent->GetMetaData(TEXT("Keywords")),
			TEXT("callable event Keywords metadata should round-trip")));

		ASSERT_THAT(IsTrue(HasAllFunctionFlags(PureConstEvent, FUNC_BlueprintEvent | FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_Const),
			TEXT("BlueprintPure BlueprintEvent const method should carry event/callable/pure/const flags")));
		ASSERT_THAT(AreEqual(FString(TEXT("PEV")), PureConstEvent->GetMetaData(TEXT("CompactNodeTitle")),
			TEXT("pure event CompactNodeTitle metadata should round-trip")));

		ASSERT_THAT(IsTrue(HasAllFunctionFlags(OutEvent, FUNC_BlueprintEvent | FUNC_BlueprintCallable | FUNC_HasOutParms),
			TEXT("BlueprintEvent with &out should carry event/callable/out flags")));
		ASSERT_THAT(AreEqual(FString(TEXT("Label")), OutEvent->GetMetaData(TEXT("AdvancedDisplay")),
			TEXT("out-param event AdvancedDisplay metadata should round-trip")));

		FIntProperty* CallableValue = CastField<FIntProperty>(FindParameterForTest(CallableEvent, TEXT("Value")));
		FIntProperty* CallableReturn = CastField<FIntProperty>(CallableEvent->GetReturnProperty());
		FIntProperty* PureValue = CastField<FIntProperty>(FindParameterForTest(PureConstEvent, TEXT("Value")));
		FIntProperty* PureReturn = CastField<FIntProperty>(PureConstEvent->GetReturnProperty());
		FIntProperty* OutValue = CastField<FIntProperty>(FindParameterForTest(OutEvent, TEXT("Value")));
		FStrProperty* OutLabel = CastField<FStrProperty>(FindParameterForTest(OutEvent, TEXT("Label")));
		FIntProperty* OutParam = CastField<FIntProperty>(FindParameterForTest(OutEvent, TEXT("OutValue")));
		FIntProperty* OutReturn = CastField<FIntProperty>(OutEvent->GetReturnProperty());
		ASSERT_THAT(IsNotNull(CallableValue, TEXT("callable event int parameter should reflect")));
		ASSERT_THAT(IsNotNull(CallableReturn, TEXT("callable event int return should reflect")));
		ASSERT_THAT(IsNotNull(PureValue, TEXT("pure event int parameter should reflect")));
		ASSERT_THAT(IsNotNull(PureReturn, TEXT("pure event int return should reflect")));
		ASSERT_THAT(IsNotNull(OutValue, TEXT("out-param event input should reflect")));
		ASSERT_THAT(IsNotNull(OutLabel, TEXT("out-param event label should reflect")));
		ASSERT_THAT(IsNotNull(OutParam, TEXT("out-param event output should reflect")));
		ASSERT_THAT(IsNotNull(OutReturn, TEXT("out-param event return should reflect")));
		if (CallableValue == nullptr || CallableReturn == nullptr || PureValue == nullptr || PureReturn == nullptr
			|| OutValue == nullptr || OutLabel == nullptr || OutParam == nullptr || OutReturn == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(CallableReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("callable event return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(PureReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("pure event return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(OutParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("event &out parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(OutParam->HasAnyPropertyFlags(CPF_ReferenceParm),
			TEXT("event &out parameter should remain out-only")));
		ASSERT_THAT(IsTrue(OutReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("out-param event return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsFalse(OutValue->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("AdvancedDisplay should not mark the first input parameter")));
		ASSERT_THAT(IsTrue(OutLabel->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("AdvancedDisplay should mark the Label parameter")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("BlueprintEvent combination actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker DirectOutInvoker(*TestRunner, Actor, TEXT("OutEvent"));
		ASSERT_THAT(IsTrue(DirectOutInvoker.IsValid(), TEXT("OutEvent should be directly invokable")));
		if (!DirectOutInvoker.IsValid())
		{
			return;
		}
		DirectOutInvoker.AddParam<int32>(25);
		DirectOutInvoker.AddParam<FString>(FString(TEXT("Direct")));
		FProperty* OutSlotProperty = nullptr;
		void* OutSlot = nullptr;
		ASSERT_THAT(IsTrue(DirectOutInvoker.AddParamSlot(OutSlotProperty, OutSlot),
			TEXT("OutEvent should expose reflected out slot")));
		FIntProperty* RuntimeOutParam = CastField<FIntProperty>(OutSlotProperty);
		ASSERT_THAT(IsNotNull(RuntimeOutParam, TEXT("OutEvent runtime out slot should be FIntProperty")));
		if (OutSlot == nullptr || RuntimeOutParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(36, DirectOutInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("direct OutEvent invocation should return script result")));
		ASSERT_THAT(AreEqual(31, RuntimeOutParam->GetPropertyValue(OutSlot),
			TEXT("direct OutEvent invocation should write out parameter")));

		FFunctionInvoker DispatchInvoker(*TestRunner, Actor, TEXT("DispatchEventCombinationMatrix"));
		ASSERT_THAT(IsTrue(DispatchInvoker.IsValid(), TEXT("DispatchEventCombinationMatrix should be invokable")));
		if (!DispatchInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(131, DispatchInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("event combination dispatch should execute plain, callable, pure, and out-param events")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("PlainEventCalls"), 1,
			TEXT("plain event should execute once through dispatcher"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallableEventCalls"), 1,
			TEXT("callable event should execute once through dispatcher"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("OutEventCalls"), 2,
			TEXT("out-param event should count direct and dispatcher calls"))));
	}

	TEST_METHOD(BlueprintEventDefaultArgumentsAndRefParameterMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_BlueprintEventDefaultRefMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionEventDefaultRefActor : AActor
			{
				UPROPERTY()
				int LastMutable = 0;

				UPROPERTY()
				FString LastLabel;

				UPROPERTY()
				int EventCallCount = 0;

				UFUNCTION(BlueprintEvent, BlueprintCallable, Category="Coverage|EventDefaults", meta=(AutoCreateRefTerm="Label"))
				int EventWithDefaults(int Value = 11, const FString&in Label = "DefaultLabel")
				{
					EventCallCount += 1;
					LastLabel = Label;
					return Value + Label.Len();
				}

				UFUNCTION(BlueprintEvent, BlueprintCallable, Category="Coverage|EventDefaults", meta=(DisplayName="Mutable Ref Event"))
				int EventWithMutableRef(UPARAM(ref) int&inout MutableValue, int Bonus = 5)
				{
					EventCallCount += 1;
					MutableValue += Bonus;
					LastMutable = MutableValue;
					return MutableValue + 1;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|EventDefaults")
				int DispatchDefaultEvents()
				{
					int Score = EventWithDefaults();
					int Mutable = 20;
					Score += EventWithMutableRef(Mutable);
					return Score + Mutable;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionBlueprintEventDefaultRefMatrix.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionEventDefaultRefActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("BlueprintEvent default/ref actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* EventWithDefaults = FindFunctionForTest(ScriptClass, TEXT("EventWithDefaults"));
		UFunction* EventWithMutableRef = FindFunctionForTest(ScriptClass, TEXT("EventWithMutableRef"));
		UFunction* DispatchDefaultEvents = FindFunctionForTest(ScriptClass, TEXT("DispatchDefaultEvents"));
		ASSERT_THAT(IsNotNull(EventWithDefaults, TEXT("EventWithDefaults should be generated")));
		ASSERT_THAT(IsNotNull(EventWithMutableRef, TEXT("EventWithMutableRef should be generated")));
		ASSERT_THAT(IsNotNull(DispatchDefaultEvents, TEXT("DispatchDefaultEvents should be generated")));
		if (EventWithDefaults == nullptr || EventWithMutableRef == nullptr || DispatchDefaultEvents == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(HasAllFunctionFlags(EventWithDefaults, FUNC_BlueprintEvent | FUNC_BlueprintCallable),
			TEXT("default BlueprintEvent should carry event and callable flags")));
		ASSERT_THAT(IsTrue(HasAllFunctionFlags(EventWithMutableRef, FUNC_BlueprintEvent | FUNC_BlueprintCallable | FUNC_HasOutParms),
			TEXT("mutable-ref BlueprintEvent should carry event/callable/out flags")));
		ASSERT_THAT(AreEqual(FString(TEXT("Label")), EventWithDefaults->GetMetaData(TEXT("AutoCreateRefTerm")),
			TEXT("event AutoCreateRefTerm metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Mutable Ref Event")), EventWithMutableRef->GetMetaData(TEXT("DisplayName")),
			TEXT("event DisplayName metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("11")), EventWithDefaults->GetMetaData(TEXT("CPP_Default_Value")),
			TEXT("BlueprintEvent int default argument should emit CPP_Default metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("DefaultLabel")), EventWithDefaults->GetMetaData(TEXT("CPP_Default_Label")),
			TEXT("BlueprintEvent FString default argument should emit CPP_Default metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("5")), EventWithMutableRef->GetMetaData(TEXT("CPP_Default_Bonus")),
			TEXT("BlueprintEvent ref function default bonus should emit CPP_Default metadata")));

		FIntProperty* DefaultValue = CastField<FIntProperty>(FindParameterForTest(EventWithDefaults, TEXT("Value")));
		FStrProperty* DefaultLabel = CastField<FStrProperty>(FindParameterForTest(EventWithDefaults, TEXT("Label")));
		FIntProperty* DefaultReturn = CastField<FIntProperty>(EventWithDefaults->GetReturnProperty());
		FIntProperty* MutableValue = CastField<FIntProperty>(FindParameterForTest(EventWithMutableRef, TEXT("MutableValue")));
		FIntProperty* Bonus = CastField<FIntProperty>(FindParameterForTest(EventWithMutableRef, TEXT("Bonus")));
		FIntProperty* MutableReturn = CastField<FIntProperty>(EventWithMutableRef->GetReturnProperty());
		ASSERT_THAT(IsNotNull(DefaultValue, TEXT("EventWithDefaults Value parameter should reflect")));
		ASSERT_THAT(IsNotNull(DefaultLabel, TEXT("EventWithDefaults Label parameter should reflect")));
		ASSERT_THAT(IsNotNull(DefaultReturn, TEXT("EventWithDefaults return should reflect")));
		ASSERT_THAT(IsNotNull(MutableValue, TEXT("EventWithMutableRef mutable value parameter should reflect")));
		ASSERT_THAT(IsNotNull(Bonus, TEXT("EventWithMutableRef bonus parameter should reflect")));
		ASSERT_THAT(IsNotNull(MutableReturn, TEXT("EventWithMutableRef return should reflect")));
		if (DefaultValue == nullptr || DefaultLabel == nullptr || DefaultReturn == nullptr
			|| MutableValue == nullptr || Bonus == nullptr || MutableReturn == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(DefaultLabel->HasAnyPropertyFlags(CPF_ConstParm | CPF_OutParm),
			TEXT("const FString&in event parameter should carry const/out reference flags")));
		ASSERT_THAT(IsTrue(MutableValue->HasAllPropertyFlags(CPF_OutParm | CPF_ReferenceParm),
			TEXT("UPARAM(ref) int&inout event parameter should carry out/reference flags")));
		ASSERT_THAT(IsFalse(MutableValue->HasAnyPropertyFlags(CPF_ConstParm),
			TEXT("UPARAM(ref) int&inout event parameter should not carry const flags")));
		ASSERT_THAT(IsTrue(DefaultReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("default event return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(MutableReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("mutable-ref event return should carry CPF_ReturnParm")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("BlueprintEvent default/ref actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker DefaultsInvoker(*TestRunner, Actor, TEXT("EventWithDefaults"));
		ASSERT_THAT(IsTrue(DefaultsInvoker.IsValid(), TEXT("EventWithDefaults should be directly invokable")));
		if (!DefaultsInvoker.IsValid())
		{
			return;
		}
		DefaultsInvoker.AddParam<int32>(9);
		DefaultsInvoker.AddParam<FString>(FString(TEXT("Direct")));
		ASSERT_THAT(AreEqual(15, DefaultsInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("direct EventWithDefaults invocation should use explicit reflected parameters")));

		FFunctionInvoker MutableInvoker(*TestRunner, Actor, TEXT("EventWithMutableRef"));
		ASSERT_THAT(IsTrue(MutableInvoker.IsValid(), TEXT("EventWithMutableRef should be directly invokable")));
		if (!MutableInvoker.IsValid())
		{
			return;
		}
		FProperty* MutableSlotProperty = nullptr;
		void* MutableSlot = nullptr;
		ASSERT_THAT(IsTrue(MutableInvoker.AddParamSlot(MutableSlotProperty, MutableSlot),
			TEXT("EventWithMutableRef should expose mutable inout slot")));
		FIntProperty* MutableSlotInt = CastField<FIntProperty>(MutableSlotProperty);
		ASSERT_THAT(IsNotNull(MutableSlotInt, TEXT("EventWithMutableRef mutable slot should be FIntProperty")));
		if (MutableSlot == nullptr || MutableSlotInt == nullptr)
		{
			return;
		}
		MutableSlotInt->SetPropertyValue(MutableSlot, 7);
		MutableInvoker.AddParam<int32>(3);
		ASSERT_THAT(AreEqual(11, MutableInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("direct EventWithMutableRef invocation should return updated mutable value plus one")));
		ASSERT_THAT(AreEqual(10, MutableSlotInt->GetPropertyValue(MutableSlot),
			TEXT("direct EventWithMutableRef invocation should mutate caller slot")));

		FFunctionInvoker DispatchInvoker(*TestRunner, Actor, TEXT("DispatchDefaultEvents"));
		ASSERT_THAT(IsTrue(DispatchInvoker.IsValid(), TEXT("DispatchDefaultEvents should be invokable")));
		if (!DispatchInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(74, DispatchInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("dispatch should execute default-argument event and mutable-ref event")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EventCallCount"), 4,
			TEXT("event call count should include direct and dispatch invocations"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastMutable"), 25,
			TEXT("dispatch mutable-ref event should store the updated local mutable value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastLabel"), FString(TEXT("DefaultLabel")),
			TEXT("dispatch default event should use FString default argument"))));
	}

	TEST_METHOD(BlueprintOverrideInheritanceMetadataMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_BlueprintOverrideMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionMetadataBase : AActor
			{
				UPROPERTY()
				int BaseCallCount = 0;

				UPROPERTY()
				int ChildCallCount = 0;

				UPROPERTY()
				FString LastLabel;

				UFUNCTION(BlueprintEvent, BlueprintCallable, Category="Coverage|Override", meta=(DisplayName="Decorated Compute", Keywords="coverage override metadata", AdvancedDisplay="Label", ToolTip="Parent metadata copied to override"))
				int DecoratedCompute(int Value, FString Label)
				{
					BaseCallCount += 1;
					LastLabel = Label;
					return Value + 1;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Override")
				int DispatchDecoratedCompute(int Value, FString Label)
				{
					return DecoratedCompute(Value, Label);
				}
			}

			UCLASS()
			class ACoverageUFunctionMetadataChild : ACoverageUFunctionMetadataBase
			{
				UFUNCTION(BlueprintOverride)
				int DecoratedCompute(int Value, FString Label)
				{
					ChildCallCount += 1;
					LastLabel = Label + ":child";
					return Value + 10;
				}
			}
			)AS");
		UClass* BaseClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionBlueprintOverrideMetadata.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionMetadataBase"));
		ASSERT_THAT(IsNotNull(BaseClass, TEXT("BlueprintOverride metadata base actor should compile")));
		if (BaseClass == nullptr)
		{
			return;
		}

		UClass* ChildClass = FindGeneratedClass(&Engine, TEXT("ACoverageUFunctionMetadataChild"));
		ASSERT_THAT(IsNotNull(ChildClass, TEXT("BlueprintOverride metadata child actor should be generated")));
		if (ChildClass == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ChildClass->IsChildOf(BaseClass),
			TEXT("BlueprintOverride metadata child should inherit from the AS base class")));

		UFunction* BaseDecoratedCompute = FindFunctionForTest(BaseClass, TEXT("DecoratedCompute"));
		UFunction* ChildDecoratedCompute = FindFunctionForTest(ChildClass, TEXT("DecoratedCompute"));
		UFunction* DispatchDecoratedCompute = FindFunctionForTest(ChildClass, TEXT("DispatchDecoratedCompute"));
		ASSERT_THAT(IsNotNull(BaseDecoratedCompute, TEXT("base BlueprintEvent function should be generated")));
		ASSERT_THAT(IsNotNull(ChildDecoratedCompute, TEXT("child BlueprintOverride function should be generated")));
		ASSERT_THAT(IsNotNull(DispatchDecoratedCompute, TEXT("inherited dispatch helper should be visible on child class")));
		if (BaseDecoratedCompute == nullptr || ChildDecoratedCompute == nullptr || DispatchDecoratedCompute == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(static_cast<UStruct*>(BaseDecoratedCompute), ChildDecoratedCompute->GetSuperStruct(),
			TEXT("child BlueprintOverride UFUNCTION should chain to the parent UFUNCTION")));
		ASSERT_THAT(IsTrue(HasAllFunctionFlags(BaseDecoratedCompute, FUNC_BlueprintEvent | FUNC_BlueprintCallable),
			TEXT("base BlueprintEvent should keep event and callable flags")));
		ASSERT_THAT(IsTrue(HasAllFunctionFlags(ChildDecoratedCompute, FUNC_BlueprintEvent | FUNC_BlueprintCallable),
			TEXT("child BlueprintOverride should inherit event and callable flags")));
		ASSERT_THAT(IsFalse(ChildDecoratedCompute->HasAnyFunctionFlags(FUNC_BlueprintPure),
			TEXT("non-pure parent event should not make child override pure")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Override")), ChildDecoratedCompute->GetMetaData(TEXT("Category")),
			TEXT("child BlueprintOverride should copy Category metadata from parent")));
		ASSERT_THAT(AreEqual(FString(TEXT("Decorated Compute")), ChildDecoratedCompute->GetMetaData(TEXT("DisplayName")),
			TEXT("child BlueprintOverride should copy DisplayName metadata from parent")));
		ASSERT_THAT(AreEqual(FString(TEXT("coverage override metadata")), ChildDecoratedCompute->GetMetaData(TEXT("Keywords")),
			TEXT("child BlueprintOverride should copy Keywords metadata from parent")));
		ASSERT_THAT(AreEqual(FString(TEXT("Label")), ChildDecoratedCompute->GetMetaData(TEXT("AdvancedDisplay")),
			TEXT("child BlueprintOverride should copy AdvancedDisplay metadata from parent")));
		ASSERT_THAT(AreEqual(FString(TEXT("Parent metadata copied to override")), ChildDecoratedCompute->GetMetaData(TEXT("ToolTip")),
			TEXT("child BlueprintOverride should copy tooltip metadata from parent")));

		FIntProperty* ChildValueParam = CastField<FIntProperty>(FindParameterForTest(ChildDecoratedCompute, TEXT("Value")));
		FStrProperty* ChildLabelParam = CastField<FStrProperty>(FindParameterForTest(ChildDecoratedCompute, TEXT("Label")));
		FIntProperty* ChildReturn = CastField<FIntProperty>(ChildDecoratedCompute->GetReturnProperty());
		ASSERT_THAT(IsNotNull(ChildValueParam, TEXT("child BlueprintOverride should expose int input parameter")));
		ASSERT_THAT(IsNotNull(ChildLabelParam, TEXT("child BlueprintOverride should expose FString input parameter")));
		ASSERT_THAT(IsNotNull(ChildReturn, TEXT("child BlueprintOverride should expose int return parameter")));
		if (ChildValueParam == nullptr || ChildLabelParam == nullptr || ChildReturn == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(ChildValueParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("Value should remain a required parameter on the child override")));
		ASSERT_THAT(IsTrue(ChildLabelParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("AdvancedDisplay metadata should mark the copied Label parameter on the child override")));
		ASSERT_THAT(IsTrue(ChildReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("child BlueprintOverride return should carry CPF_ReturnParm")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ChildClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("BlueprintOverride metadata child actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker DirectOverrideInvoker(*TestRunner, Actor, TEXT("DecoratedCompute"));
		ASSERT_THAT(IsTrue(DirectOverrideInvoker.IsValid(), TEXT("child BlueprintOverride should be directly invokable")));
		if (!DirectOverrideInvoker.IsValid())
		{
			return;
		}
		DirectOverrideInvoker.AddParam<int32>(20);
		DirectOverrideInvoker.AddParam<FString>(FString(TEXT("direct")));
		ASSERT_THAT(AreEqual(30, DirectOverrideInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("direct child BlueprintOverride invocation should execute child implementation")));

		FFunctionInvoker DispatchInvoker(*TestRunner, Actor, TEXT("DispatchDecoratedCompute"));
		ASSERT_THAT(IsTrue(DispatchInvoker.IsValid(), TEXT("inherited dispatch helper should be invokable on child actor")));
		if (!DispatchInvoker.IsValid())
		{
			return;
		}
		DispatchInvoker.AddParam<int32>(32);
		DispatchInvoker.AddParam<FString>(FString(TEXT("dispatch")));
		ASSERT_THAT(AreEqual(42, DispatchInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("base dispatch helper should route virtual call to child BlueprintOverride implementation")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BaseCallCount"), 0,
			TEXT("child override calls should not execute the parent event body without Super"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ChildCallCount"), 2,
			TEXT("child override implementation should execute for direct and dispatch calls"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastLabel"), FString(TEXT("dispatch:child")),
			TEXT("child override should receive and store the dispatch label"))));
	}

	TEST_METHOD(BlueprintOverrideOutParameterAndSuperMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_BlueprintOverrideOutSuper"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionOutOverrideBase : AActor
			{
				UPROPERTY()
				int BaseCallCount = 0;

				UPROPERTY()
				int ChildCallCount = 0;

				UPROPERTY()
				int LastOutValue = 0;

				UFUNCTION(BlueprintEvent, BlueprintCallable, Category="Coverage|OutOverride", meta=(AdvancedDisplay="Label", DisplayName="Compute Out Value"))
				int ComputeOutValue(int Input, FString Label, int&out OutValue)
				{
					BaseCallCount += 1;
					OutValue = Input + Label.Len();
					LastOutValue = OutValue;
					return OutValue + 1;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|OutOverride")
				int DispatchOutValue(int Input, FString Label, int&out OutValue)
				{
					return ComputeOutValue(Input, Label, OutValue);
				}
			}

			UCLASS()
			class ACoverageUFunctionOutOverrideChild : ACoverageUFunctionOutOverrideBase
			{
				UFUNCTION(BlueprintOverride)
				int ComputeOutValue(int Input, FString Label, int&out OutValue)
				{
					ChildCallCount += 1;

					int ParentOut = 0;
					int ParentReturn = Super::ComputeOutValue(Input, Label, ParentOut);
					OutValue = ParentOut + 4;
					LastOutValue = OutValue;
					return ParentReturn + OutValue;
				}
			}
			)AS");
		UClass* BaseClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionBlueprintOverrideOutSuper.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionOutOverrideBase"));
		ASSERT_THAT(IsNotNull(BaseClass, TEXT("BlueprintOverride out-param base actor should compile")));
		if (BaseClass == nullptr)
		{
			return;
		}

		UClass* ChildClass = FindGeneratedClass(&Engine, TEXT("ACoverageUFunctionOutOverrideChild"));
		ASSERT_THAT(IsNotNull(ChildClass, TEXT("BlueprintOverride out-param child actor should be generated")));
		if (ChildClass == nullptr)
		{
			return;
		}

		UFunction* BaseComputeOutValue = FindFunctionForTest(BaseClass, TEXT("ComputeOutValue"));
		UFunction* ChildComputeOutValue = FindFunctionForTest(ChildClass, TEXT("ComputeOutValue"));
		UFunction* DispatchOutValue = FindFunctionForTest(ChildClass, TEXT("DispatchOutValue"));
		ASSERT_THAT(IsNotNull(BaseComputeOutValue, TEXT("base out-param BlueprintEvent should be generated")));
		ASSERT_THAT(IsNotNull(ChildComputeOutValue, TEXT("child out-param BlueprintOverride should be generated")));
		ASSERT_THAT(IsNotNull(DispatchOutValue, TEXT("out-param dispatch helper should be visible on child class")));
		if (BaseComputeOutValue == nullptr || ChildComputeOutValue == nullptr || DispatchOutValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(static_cast<UStruct*>(BaseComputeOutValue), ChildComputeOutValue->GetSuperStruct(),
			TEXT("child out-param override should chain to the parent UFUNCTION")));
		ASSERT_THAT(IsTrue(HasAllFunctionFlags(ChildComputeOutValue, FUNC_BlueprintEvent | FUNC_BlueprintCallable | FUNC_HasOutParms),
			TEXT("child out-param override should inherit event/callable/out flags")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|OutOverride")), ChildComputeOutValue->GetMetaData(TEXT("Category")),
			TEXT("child out-param override should copy Category metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Compute Out Value")), ChildComputeOutValue->GetMetaData(TEXT("DisplayName")),
			TEXT("child out-param override should copy DisplayName metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Label")), ChildComputeOutValue->GetMetaData(TEXT("AdvancedDisplay")),
			TEXT("child out-param override should copy AdvancedDisplay metadata")));

		FIntProperty* ChildInputParam = CastField<FIntProperty>(FindParameterForTest(ChildComputeOutValue, TEXT("Input")));
		FStrProperty* ChildLabelParam = CastField<FStrProperty>(FindParameterForTest(ChildComputeOutValue, TEXT("Label")));
		FIntProperty* ChildOutParam = CastField<FIntProperty>(FindParameterForTest(ChildComputeOutValue, TEXT("OutValue")));
		FIntProperty* ChildReturnParam = CastField<FIntProperty>(ChildComputeOutValue->GetReturnProperty());
		ASSERT_THAT(IsNotNull(ChildInputParam, TEXT("child out-param override should expose Input parameter")));
		ASSERT_THAT(IsNotNull(ChildLabelParam, TEXT("child out-param override should expose Label parameter")));
		ASSERT_THAT(IsNotNull(ChildOutParam, TEXT("child out-param override should expose OutValue parameter")));
		ASSERT_THAT(IsNotNull(ChildReturnParam, TEXT("child out-param override should expose return parameter")));
		if (ChildInputParam == nullptr || ChildLabelParam == nullptr || ChildOutParam == nullptr || ChildReturnParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(ChildInputParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("Input should remain a required parameter on copied out-param override metadata")));
		ASSERT_THAT(IsTrue(ChildLabelParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("copied AdvancedDisplay metadata should mark Label")));
		ASSERT_THAT(IsTrue(ChildOutParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("child out-param override should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(ChildOutParam->HasAnyPropertyFlags(CPF_ReferenceParm),
			TEXT("child out-param override should keep OutValue as out-only")));
		ASSERT_THAT(IsTrue(ChildReturnParam->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("child out-param override return should carry CPF_ReturnParm")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ChildClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("BlueprintOverride out-param child actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker DirectInvoker(*TestRunner, Actor, TEXT("ComputeOutValue"));
		ASSERT_THAT(IsTrue(DirectInvoker.IsValid(), TEXT("child out-param override should be directly invokable")));
		if (!DirectInvoker.IsValid())
		{
			return;
		}
		DirectInvoker.AddParam<int32>(20);
		DirectInvoker.AddParam<FString>(FString(TEXT("abc")));
		FProperty* DirectOutSlotProperty = nullptr;
		void* DirectOutSlot = nullptr;
		ASSERT_THAT(IsTrue(DirectInvoker.AddParamSlot(DirectOutSlotProperty, DirectOutSlot),
			TEXT("child direct out-param override should expose out slot")));
		FIntProperty* DirectOutSlotInt = CastField<FIntProperty>(DirectOutSlotProperty);
		ASSERT_THAT(IsNotNull(DirectOutSlotInt, TEXT("child direct out-param slot should be FIntProperty")));
		if (DirectOutSlot == nullptr || DirectOutSlotInt == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(51, DirectInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("child direct out-param override should combine Super return and child out value")));
		ASSERT_THAT(AreEqual(27, DirectOutSlotInt->GetPropertyValue(DirectOutSlot),
			TEXT("child direct out-param override should write mutated out value")));

		FFunctionInvoker DispatchInvoker(*TestRunner, Actor, TEXT("DispatchOutValue"));
		ASSERT_THAT(IsTrue(DispatchInvoker.IsValid(), TEXT("out-param dispatch helper should be invokable")));
		if (!DispatchInvoker.IsValid())
		{
			return;
		}
		DispatchInvoker.AddParam<int32>(15);
		DispatchInvoker.AddParam<FString>(FString(TEXT("abcd")));
		FProperty* DispatchOutSlotProperty = nullptr;
		void* DispatchOutSlot = nullptr;
		ASSERT_THAT(IsTrue(DispatchInvoker.AddParamSlot(DispatchOutSlotProperty, DispatchOutSlot),
			TEXT("dispatch helper should expose out slot")));
		FIntProperty* DispatchOutSlotInt = CastField<FIntProperty>(DispatchOutSlotProperty);
		ASSERT_THAT(IsNotNull(DispatchOutSlotInt, TEXT("dispatch out slot should be FIntProperty")));
		if (DispatchOutSlot == nullptr || DispatchOutSlotInt == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(43, DispatchInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("dispatch helper should route virtual out-param call to child override")));
		ASSERT_THAT(AreEqual(23, DispatchOutSlotInt->GetPropertyValue(DispatchOutSlot),
			TEXT("dispatch helper should receive child-written out value")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BaseCallCount"), 2,
			TEXT("Super::ComputeOutValue should execute parent body for direct and dispatch calls"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ChildCallCount"), 2,
			TEXT("child out-param override should execute for direct and dispatch calls"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastOutValue"), 23,
			TEXT("child out-param override should store the last child-written out value"))));
	}

	TEST_METHOD(ConstBlueprintPureOverrideMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_ConstPureOverride"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionPureOverrideBase : AActor
			{
				UPROPERTY()
				int StoredValue = 7;

				UFUNCTION(BlueprintPure, BlueprintEvent, Category="Coverage|PureOverride", meta=(DisplayName="Compute Pure Value", CompactNodeTitle="PURE", AdvancedDisplay="Bias"))
				int ComputePureValue(int Scale, int Bias = 1) const
				{
					return StoredValue * Scale + Bias;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|PureOverride")
				int DispatchPureValue(int Scale, int Bias = 1) const
				{
					return ComputePureValue(Scale, Bias);
				}
			}

			UCLASS()
			class ACoverageUFunctionPureOverrideChild : ACoverageUFunctionPureOverrideBase
			{
				UFUNCTION(BlueprintOverride)
				int ComputePureValue(int Scale, int Bias = 1) const
				{
					return StoredValue * Scale + Bias + 10;
				}
			}
			)AS");
		UClass* BaseClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionConstPureOverride.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionPureOverrideBase"));
		ASSERT_THAT(IsNotNull(BaseClass, TEXT("const pure override base actor should compile")));
		if (BaseClass == nullptr)
		{
			return;
		}

		UClass* ChildClass = FindGeneratedClass(&Engine, TEXT("ACoverageUFunctionPureOverrideChild"));
		ASSERT_THAT(IsNotNull(ChildClass, TEXT("const pure override child actor should be generated")));
		if (ChildClass == nullptr)
		{
			return;
		}

		UFunction* BaseComputePureValue = FindFunctionForTest(BaseClass, TEXT("ComputePureValue"));
		UFunction* ChildComputePureValue = FindFunctionForTest(ChildClass, TEXT("ComputePureValue"));
		UFunction* DispatchPureValue = FindFunctionForTest(ChildClass, TEXT("DispatchPureValue"));
		ASSERT_THAT(IsNotNull(BaseComputePureValue, TEXT("base BlueprintPure BlueprintEvent should be generated")));
		ASSERT_THAT(IsNotNull(ChildComputePureValue, TEXT("child BlueprintOverride of pure event should be generated")));
		ASSERT_THAT(IsNotNull(DispatchPureValue, TEXT("inherited pure dispatch helper should be visible on child class")));
		if (BaseComputePureValue == nullptr || ChildComputePureValue == nullptr || DispatchPureValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(HasAllFunctionFlags(BaseComputePureValue, FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_BlueprintEvent | FUNC_Const),
			TEXT("base pure event should carry callable, pure, event, and const flags")));
		ASSERT_THAT(IsTrue(HasAllFunctionFlags(ChildComputePureValue, FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_BlueprintEvent | FUNC_Const),
			TEXT("child override should inherit pure, event, callable, and const flags")));
		ASSERT_THAT(AreEqual(static_cast<UStruct*>(BaseComputePureValue), ChildComputePureValue->GetSuperStruct(),
			TEXT("child pure override should chain to the parent UFunction")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|PureOverride")), ChildComputePureValue->GetMetaData(TEXT("Category")),
			TEXT("child pure override should copy Category metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Compute Pure Value")), ChildComputePureValue->GetMetaData(TEXT("DisplayName")),
			TEXT("child pure override should copy DisplayName metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("PURE")), ChildComputePureValue->GetMetaData(TEXT("CompactNodeTitle")),
			TEXT("child pure override should copy CompactNodeTitle metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Bias")), ChildComputePureValue->GetMetaData(TEXT("AdvancedDisplay")),
			TEXT("child pure override should copy AdvancedDisplay metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("1")), ChildComputePureValue->GetMetaData(TEXT("CPP_Default_Bias")),
			TEXT("child pure override should preserve copied default parameter metadata")));

		FIntProperty* ChildScaleParam = CastField<FIntProperty>(FindParameterForTest(ChildComputePureValue, TEXT("Scale")));
		FIntProperty* ChildBiasParam = CastField<FIntProperty>(FindParameterForTest(ChildComputePureValue, TEXT("Bias")));
		FIntProperty* ChildReturnParam = CastField<FIntProperty>(ChildComputePureValue->GetReturnProperty());
		ASSERT_THAT(IsNotNull(ChildScaleParam, TEXT("child pure override should expose Scale parameter")));
		ASSERT_THAT(IsNotNull(ChildBiasParam, TEXT("child pure override should expose Bias parameter")));
		ASSERT_THAT(IsNotNull(ChildReturnParam, TEXT("child pure override should expose return parameter")));
		if (ChildScaleParam == nullptr || ChildBiasParam == nullptr || ChildReturnParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(ChildScaleParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("Scale should remain a required parameter")));
		ASSERT_THAT(IsTrue(ChildBiasParam->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("copied AdvancedDisplay should mark Bias on the child override")));
		ASSERT_THAT(IsTrue(ChildReturnParam->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("child pure override return should carry CPF_ReturnParm")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ChildClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("const pure override child actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker DirectInvoker(*TestRunner, Actor, TEXT("ComputePureValue"));
		ASSERT_THAT(IsTrue(DirectInvoker.IsValid(), TEXT("child pure override should be directly invokable")));
		if (!DirectInvoker.IsValid())
		{
			return;
		}
		DirectInvoker.AddParam<int32>(4);
		DirectInvoker.AddParam<int32>(4);
		ASSERT_THAT(AreEqual(42, DirectInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("direct pure override invocation should execute child body")));

		FFunctionInvoker DispatchInvoker(*TestRunner, Actor, TEXT("DispatchPureValue"));
		ASSERT_THAT(IsTrue(DispatchInvoker.IsValid(), TEXT("inherited pure dispatch helper should be invokable")));
		if (!DispatchInvoker.IsValid())
		{
			return;
		}
		DispatchInvoker.AddParam<int32>(5);
		DispatchInvoker.AddParam<int32>(-3);
		ASSERT_THAT(AreEqual(42, DispatchInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("base const dispatch helper should route to the child pure override")));
	}

	TEST_METHOD(RecursionAndVirtualOverrideDispatch)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_RecursionVirtualSuper"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* BaseClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionRecursionVirtualSuper.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionVirtualBase : AActor
			{
				UPROPERTY()
				int TraceValue = 0;

				UFUNCTION(BlueprintEvent)
				int ComputeVirtual(int Value)
				{
					TraceValue = TraceValue * 10 + 1;
					return Value + 1;
				}

				UFUNCTION()
				int Factorial(int Value)
				{
					if (Value <= 1)
					{
						return 1;
					}

					return Value * Factorial(Value - 1);
				}

				UFUNCTION()
				int DispatchVirtual(int Value)
				{
					return ComputeVirtual(Value);
				}
			}

			UCLASS()
			class ACoverageUFunctionVirtualChild : ACoverageUFunctionVirtualBase
			{
				UFUNCTION(BlueprintOverride)
				int ComputeVirtual(int Value)
				{
					TraceValue = TraceValue * 10 + 2;
					return Value + 10;
				}
			}
			)AS"),
			TEXT("ACoverageUFunctionVirtualBase"));
		ASSERT_THAT(IsNotNull(BaseClass, TEXT("Virtual UFUNCTION base class should compile")));
		if (BaseClass == nullptr)
		{
			return;
		}

		UClass* ChildClass = FindGeneratedClass(&Engine, TEXT("ACoverageUFunctionVirtualChild"));
		ASSERT_THAT(IsNotNull(ChildClass, TEXT("Virtual UFUNCTION child class should be generated")));
		if (ChildClass == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ChildClass->IsChildOf(BaseClass), TEXT("Virtual child class should inherit from base class")));

		UFunction* BaseComputeVirtual = FindFunctionForTest(BaseClass, TEXT("ComputeVirtual"));
		UFunction* ChildComputeVirtual = FindFunctionForTest(ChildClass, TEXT("ComputeVirtual"));
		UFunction* FactorialFunction = FindFunctionForTest(BaseClass, TEXT("Factorial"));
		ASSERT_THAT(IsNotNull(BaseComputeVirtual, TEXT("Base BlueprintEvent virtual method should be generated")));
		ASSERT_THAT(IsNotNull(ChildComputeVirtual, TEXT("Child BlueprintOverride virtual method should be generated")));
		ASSERT_THAT(IsNotNull(FactorialFunction, TEXT("Recursive UFUNCTION should be generated")));
		if (BaseComputeVirtual == nullptr || ChildComputeVirtual == nullptr || FactorialFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(BaseComputeVirtual->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("Base virtual method should be a BlueprintEvent")));
		ASSERT_THAT(IsTrue(ChildComputeVirtual->HasAnyFunctionFlags(FUNC_BlueprintEvent),
			TEXT("Child virtual override should surface as a BlueprintEvent")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BaseActor = SpawnScriptActor(*TestRunner, Spawner, BaseClass);
		ASSERT_THAT(IsNotNull(BaseActor, TEXT("Virtual base actor should spawn")));
		if (BaseActor == nullptr)
		{
			return;
		}

		AActor* ChildActor = SpawnScriptActor(*TestRunner, Spawner, ChildClass);
		ASSERT_THAT(IsNotNull(ChildActor, TEXT("Virtual child actor should spawn")));
		if (ChildActor == nullptr)
		{
			return;
		}

		FFunctionInvoker BaseDispatchInvoker(*TestRunner, BaseActor, TEXT("DispatchVirtual"));
		ASSERT_THAT(IsTrue(BaseDispatchInvoker.IsValid(), TEXT("Base virtual dispatch helper should be invokable through reflection")));
		if (!BaseDispatchInvoker.IsValid())
		{
			return;
		}
		BaseDispatchInvoker.AddParam<int32>(7);
		ASSERT_THAT(AreEqual(8, BaseDispatchInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Base call site should execute the parent BlueprintEvent implementation")));

		FFunctionInvoker FactorialInvoker(*TestRunner, ChildActor, TEXT("Factorial"));
		ASSERT_THAT(IsTrue(FactorialInvoker.IsValid(), TEXT("Recursive UFUNCTION should be invokable through reflection")));
		if (!FactorialInvoker.IsValid())
		{
			return;
		}
		FactorialInvoker.AddParam<int32>(5);
		ASSERT_THAT(AreEqual(120, FactorialInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Recursive UFUNCTION should call itself until the base case")));

		FFunctionInvoker DispatchInvoker(*TestRunner, ChildActor, TEXT("DispatchVirtual"));
		ASSERT_THAT(IsTrue(DispatchInvoker.IsValid(), TEXT("Virtual dispatch helper should be invokable through reflection")));
		if (!DispatchInvoker.IsValid())
		{
			return;
		}
		DispatchInvoker.AddParam<int32>(7);
		ASSERT_THAT(AreEqual(17, DispatchInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("Inherited base call site should dispatch to child BlueprintOverride")));

		int32 BaseTraceValue = 0;
		int32 TraceValue = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, BaseActor, TEXT("TraceValue"), BaseTraceValue),
			TEXT("Base TraceValue should be readable after virtual dispatch")));
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, ChildActor, TEXT("TraceValue"), TraceValue),
			TEXT("TraceValue should be readable after virtual dispatch")));
		ASSERT_THAT(AreEqual(1, BaseTraceValue,
			TEXT("Base virtual dispatch should run the parent BlueprintEvent implementation")));
		ASSERT_THAT(AreEqual(2, TraceValue,
			TEXT("Inherited virtual dispatch should run only the child BlueprintOverride implementation")));
	}

	TEST_METHOD(ReturnTypeReflectionMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_ReturnTypeMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionReturnTypeMatrix.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionReturnMatrixActor : AActor
			{
				UFUNCTION()
				bool ReturnBool()
				{
					return true;
				}

				UFUNCTION()
				double ReturnNumber()
				{
					return 12.5;
				}

				UFUNCTION()
				FString ReturnString()
				{
					return "coverage";
				}

				UFUNCTION()
				FVector ReturnVector()
				{
					return FVector(1.0, 2.0, 3.0);
				}

				UFUNCTION()
				AActor ReturnSelfActor()
				{
					return this;
				}
			}
			)AS"),
			TEXT("ACoverageUFunctionReturnMatrixActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UFUNCTION return matrix actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* ReturnBool = FindFunctionForTest(ScriptClass, TEXT("ReturnBool"));
		UFunction* ReturnNumber = FindFunctionForTest(ScriptClass, TEXT("ReturnNumber"));
		UFunction* ReturnString = FindFunctionForTest(ScriptClass, TEXT("ReturnString"));
		UFunction* ReturnVector = FindFunctionForTest(ScriptClass, TEXT("ReturnVector"));
		UFunction* ReturnSelfActor = FindFunctionForTest(ScriptClass, TEXT("ReturnSelfActor"));
		ASSERT_THAT(IsNotNull(ReturnBool, TEXT("bool return UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnNumber, TEXT("double return UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnString, TEXT("FString return UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnVector, TEXT("FVector return UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnSelfActor, TEXT("AActor return UFUNCTION should be generated")));
		if (ReturnBool == nullptr || ReturnNumber == nullptr || ReturnString == nullptr || ReturnVector == nullptr || ReturnSelfActor == nullptr)
		{
			return;
		}

		FProperty* BoolReturn = FindParameterForTest(ReturnBool, TEXT("ReturnValue"));
		FProperty* NumberReturn = FindParameterForTest(ReturnNumber, TEXT("ReturnValue"));
		FProperty* StringReturn = FindParameterForTest(ReturnString, TEXT("ReturnValue"));
		FProperty* VectorReturn = FindParameterForTest(ReturnVector, TEXT("ReturnValue"));
		FProperty* ActorReturn = FindParameterForTest(ReturnSelfActor, TEXT("ReturnValue"));
		ASSERT_THAT(IsNotNull(BoolReturn, TEXT("bool return parameter should be reflected")));
		ASSERT_THAT(IsNotNull(NumberReturn, TEXT("double return parameter should be reflected")));
		ASSERT_THAT(IsNotNull(StringReturn, TEXT("FString return parameter should be reflected")));
		ASSERT_THAT(IsNotNull(VectorReturn, TEXT("FVector return parameter should be reflected")));
		ASSERT_THAT(IsNotNull(ActorReturn, TEXT("AActor return parameter should be reflected")));
		if (BoolReturn == nullptr || NumberReturn == nullptr || StringReturn == nullptr || VectorReturn == nullptr || ActorReturn == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(BoolReturn->HasAnyPropertyFlags(CPF_ReturnParm), TEXT("bool return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(NumberReturn->HasAnyPropertyFlags(CPF_ReturnParm), TEXT("double return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(StringReturn->HasAnyPropertyFlags(CPF_ReturnParm), TEXT("FString return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(VectorReturn->HasAnyPropertyFlags(CPF_ReturnParm), TEXT("FVector return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(ActorReturn->HasAnyPropertyFlags(CPF_ReturnParm), TEXT("AActor return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsNotNull(CastField<FBoolProperty>(BoolReturn), TEXT("bool return should reflect as FBoolProperty")));
		ASSERT_THAT(IsNotNull(CastField<FDoubleProperty>(NumberReturn), TEXT("double return should reflect as FDoubleProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(StringReturn), TEXT("FString return should reflect as FStrProperty")));

		FStructProperty* VectorReturnProperty = CastField<FStructProperty>(VectorReturn);
		ASSERT_THAT(IsNotNull(VectorReturnProperty, TEXT("FVector return should reflect as FStructProperty")));
		if (VectorReturnProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(TBaseStructure<FVector>::Get(), VectorReturnProperty->Struct,
			TEXT("FVector return should preserve the FVector struct type")));

		FObjectProperty* ActorReturnProperty = CastField<FObjectProperty>(ActorReturn);
		ASSERT_THAT(IsNotNull(ActorReturnProperty, TEXT("AActor return should reflect as FObjectProperty")));
		if (ActorReturnProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(AActor::StaticClass(), ActorReturnProperty->PropertyClass,
			TEXT("AActor return should preserve the reflected object class")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UFUNCTION return matrix actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker BoolInvoker(*TestRunner, Actor, TEXT("ReturnBool"));
		ASSERT_THAT(IsTrue(BoolInvoker.IsValid(), TEXT("ReturnBool should be invokable through reflection")));
		if (!BoolInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(BoolInvoker.CallAndReturn<bool>(false), TEXT("bool reflected return should round-trip")));

		FFunctionInvoker NumberInvoker(*TestRunner, Actor, TEXT("ReturnNumber"));
		ASSERT_THAT(IsTrue(NumberInvoker.IsValid(), TEXT("ReturnNumber should be invokable through reflection")));
		if (!NumberInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(12.5, NumberInvoker.CallAndReturn<double>(0.0)),
			TEXT("double reflected return should round-trip")));

		FFunctionInvoker StringInvoker(*TestRunner, Actor, TEXT("ReturnString"));
		ASSERT_THAT(IsTrue(StringInvoker.IsValid(), TEXT("ReturnString should be invokable through reflection")));
		if (!StringInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("coverage")), StringInvoker.CallAndReturn<FString>(FString()),
			TEXT("FString reflected return should round-trip")));

		FFunctionInvoker VectorInvoker(*TestRunner, Actor, TEXT("ReturnVector"));
		ASSERT_THAT(IsTrue(VectorInvoker.IsValid(), TEXT("ReturnVector should be invokable through reflection")));
		if (!VectorInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(FVector(1.0, 2.0, 3.0), VectorInvoker.CallAndReturn<FVector>(FVector::ZeroVector),
			TEXT("FVector reflected return should round-trip")));

		FFunctionInvoker ActorInvoker(*TestRunner, Actor, TEXT("ReturnSelfActor"));
		ASSERT_THAT(IsTrue(ActorInvoker.IsValid(), TEXT("ReturnSelfActor should be invokable through reflection")));
		if (!ActorInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(Actor, ActorInvoker.CallAndReturn<AActor*>(nullptr),
			TEXT("AActor reflected return should round-trip")));
	}

	TEST_METHOD(ObjectAndClassParameterReflectionMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_ObjectClassParameters"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionObjectBaseActor : AActor
			{
			}

			UCLASS()
			class ACoverageUFunctionObjectClassActor : ACoverageUFunctionObjectBaseActor
			{
				UPROPERTY()
				UObject LastObject;

				UPROPERTY()
				AActor LastActor;

				UPROPERTY()
				UClass LastClass;

				UPROPERTY()
				TSubclassOf<AActor> LastActorClass;

				UFUNCTION(BlueprintCallable, Category="Coverage|ObjectClass")
				int AcceptObjectClassMatrix(UObject ObjectValue, AActor ActorValue, UClass ClassValue, TSubclassOf<AActor> ActorClassValue)
				{
					LastObject = ObjectValue;
					LastActor = ActorValue;
					LastClass = ClassValue;
					LastActorClass = ActorClassValue;

					int Score = 0;
					if (LastObject == this)
					{
						Score += 1;
					}
					if (LastActor == this)
					{
						Score += 2;
					}
					if (LastClass == ACoverageUFunctionObjectClassActor::StaticClass())
					{
						Score += 4;
					}
					if (LastActorClass != nullptr && LastActorClass.IsChildOf(ACoverageUFunctionObjectBaseActor::StaticClass()))
					{
						Score += 8;
					}
					return Score;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|ObjectClass")
				UObject ReturnSelfAsObject()
				{
					return this;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|ObjectClass")
				UClass ReturnSelfClass()
				{
					return ACoverageUFunctionObjectClassActor::StaticClass();
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|ObjectClass")
				TSubclassOf<AActor> ReturnActorSubclass()
				{
					return ACoverageUFunctionObjectClassActor::StaticClass();
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionObjectClassParameters.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionObjectClassActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("object/class UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UClass* BaseClass = FindGeneratedClass(&Engine, TEXT("ACoverageUFunctionObjectBaseActor"));
		ASSERT_THAT(IsNotNull(BaseClass, TEXT("object/class base actor should be generated")));
		if (BaseClass == nullptr)
		{
			return;
		}

		UFunction* AcceptObjectClassMatrix = FindFunctionForTest(ScriptClass, TEXT("AcceptObjectClassMatrix"));
		UFunction* ReturnSelfAsObject = FindFunctionForTest(ScriptClass, TEXT("ReturnSelfAsObject"));
		UFunction* ReturnSelfClass = FindFunctionForTest(ScriptClass, TEXT("ReturnSelfClass"));
		UFunction* ReturnActorSubclass = FindFunctionForTest(ScriptClass, TEXT("ReturnActorSubclass"));
		ASSERT_THAT(IsNotNull(AcceptObjectClassMatrix, TEXT("object/class matrix UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnSelfAsObject, TEXT("object return UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnSelfClass, TEXT("UClass return UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnActorSubclass, TEXT("TSubclassOf return UFUNCTION should be generated")));
		if (AcceptObjectClassMatrix == nullptr || ReturnSelfAsObject == nullptr || ReturnSelfClass == nullptr || ReturnActorSubclass == nullptr)
		{
			return;
		}

		FObjectProperty* ObjectParam = CastField<FObjectProperty>(FindParameterForTest(AcceptObjectClassMatrix, TEXT("ObjectValue")));
		FObjectProperty* ActorParam = CastField<FObjectProperty>(FindParameterForTest(AcceptObjectClassMatrix, TEXT("ActorValue")));
		FClassProperty* ClassParam = CastField<FClassProperty>(FindParameterForTest(AcceptObjectClassMatrix, TEXT("ClassValue")));
		FClassProperty* ActorClassParam = CastField<FClassProperty>(FindParameterForTest(AcceptObjectClassMatrix, TEXT("ActorClassValue")));
		FIntProperty* ReturnScore = CastField<FIntProperty>(FindParameterForTest(AcceptObjectClassMatrix, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(ObjectParam, TEXT("UObject parameter should reflect as FObjectProperty")));
		ASSERT_THAT(IsNotNull(ActorParam, TEXT("AActor parameter should reflect as FObjectProperty")));
		ASSERT_THAT(IsNotNull(ClassParam, TEXT("UClass parameter should reflect as FClassProperty")));
		ASSERT_THAT(IsNotNull(ActorClassParam, TEXT("TSubclassOf<AActor> parameter should reflect as FClassProperty")));
		ASSERT_THAT(IsNotNull(ReturnScore, TEXT("object/class matrix return should reflect as FIntProperty")));
		if (ObjectParam == nullptr || ActorParam == nullptr || ClassParam == nullptr || ActorClassParam == nullptr || ReturnScore == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(UObject::StaticClass(), ObjectParam->PropertyClass,
			TEXT("UObject parameter should preserve UObject class")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), ActorParam->PropertyClass,
			TEXT("AActor parameter should preserve AActor class")));
		ASSERT_THAT(AreEqual(UObject::StaticClass(), ClassParam->MetaClass,
			TEXT("plain UClass parameter should allow UObject classes")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), ActorClassParam->MetaClass,
			TEXT("TSubclassOf<AActor> parameter should constrain MetaClass to AActor")));
		ASSERT_THAT(IsTrue(ReturnScore->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("object/class matrix return should carry CPF_ReturnParm")));

		FObjectProperty* ObjectReturn = CastField<FObjectProperty>(FindParameterForTest(ReturnSelfAsObject, TEXT("ReturnValue")));
		FClassProperty* ClassReturn = CastField<FClassProperty>(FindParameterForTest(ReturnSelfClass, TEXT("ReturnValue")));
		FClassProperty* SubclassReturn = CastField<FClassProperty>(FindParameterForTest(ReturnActorSubclass, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(ObjectReturn, TEXT("UObject return should reflect as FObjectProperty")));
		ASSERT_THAT(IsNotNull(ClassReturn, TEXT("UClass return should reflect as FClassProperty")));
		ASSERT_THAT(IsNotNull(SubclassReturn, TEXT("TSubclassOf<AActor> return should reflect as FClassProperty")));
		if (ObjectReturn == nullptr || ClassReturn == nullptr || SubclassReturn == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(UObject::StaticClass(), ObjectReturn->PropertyClass,
			TEXT("UObject return should preserve UObject class")));
		ASSERT_THAT(AreEqual(UObject::StaticClass(), ClassReturn->MetaClass,
			TEXT("UClass return should allow UObject classes")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), SubclassReturn->MetaClass,
			TEXT("TSubclassOf<AActor> return should constrain MetaClass to AActor")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("object/class UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker AcceptInvoker(*TestRunner, Actor, TEXT("AcceptObjectClassMatrix"));
		ASSERT_THAT(IsTrue(AcceptInvoker.IsValid(), TEXT("AcceptObjectClassMatrix should be invokable through reflection")));
		if (!AcceptInvoker.IsValid())
		{
			return;
		}
		AcceptInvoker.AddParam<UObject*>(Actor);
		AcceptInvoker.AddParam<AActor*>(Actor);
		AcceptInvoker.AddParam<UClass*>(ScriptClass);
		AcceptInvoker.AddParam<TSubclassOf<AActor>>(TSubclassOf<AActor>(ScriptClass));
		ASSERT_THAT(AreEqual(15, AcceptInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("object/class matrix reflected invocation should consume all object and class-shaped inputs")));

		UObject* LastObject = nullptr;
		UObject* LastActor = nullptr;
		UClass* LastClass = nullptr;
		UClass* LastActorClass = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("LastObject"), LastObject),
			TEXT("LastObject should be readable after object/class call")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("LastActor"), LastActor),
			TEXT("LastActor should be readable after object/class call")));
		ASSERT_THAT(IsTrue(GetClassByPath(*TestRunner, Actor, TEXT("LastClass"), LastClass),
			TEXT("LastClass should be readable after object/class call")));
		ASSERT_THAT(IsTrue(GetClassByPath(*TestRunner, Actor, TEXT("LastActorClass"), LastActorClass),
			TEXT("LastActorClass should be readable after object/class call")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), LastObject,
			TEXT("UObject parameter should round-trip into script state")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), LastActor,
			TEXT("AActor parameter should round-trip into script state")));
		ASSERT_THAT(AreEqual(ScriptClass, LastClass,
			TEXT("UClass parameter should round-trip into script state")));
		ASSERT_THAT(AreEqual(ScriptClass, LastActorClass,
			TEXT("TSubclassOf<AActor> parameter should round-trip into script state")));

		FFunctionInvoker ObjectReturnInvoker(*TestRunner, Actor, TEXT("ReturnSelfAsObject"));
		ASSERT_THAT(IsTrue(ObjectReturnInvoker.IsValid(), TEXT("ReturnSelfAsObject should be invokable")));
		if (!ObjectReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), ObjectReturnInvoker.CallAndReturn<UObject*>(nullptr),
			TEXT("UObject return should round-trip through reflection")));

		FFunctionInvoker ClassReturnInvoker(*TestRunner, Actor, TEXT("ReturnSelfClass"));
		ASSERT_THAT(IsTrue(ClassReturnInvoker.IsValid(), TEXT("ReturnSelfClass should be invokable")));
		if (!ClassReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(ScriptClass, ClassReturnInvoker.CallAndReturn<UClass*>(nullptr),
			TEXT("UClass return should round-trip through reflection")));

		FFunctionInvoker SubclassReturnInvoker(*TestRunner, Actor, TEXT("ReturnActorSubclass"));
		ASSERT_THAT(IsTrue(SubclassReturnInvoker.IsValid(), TEXT("ReturnActorSubclass should be invokable")));
		if (!SubclassReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(ScriptClass, SubclassReturnInvoker.CallAndReturn<TSubclassOf<AActor>>(TSubclassOf<AActor>()).Get(),
			TEXT("TSubclassOf<AActor> return should round-trip through reflection")));
	}

	TEST_METHOD(EnumParameterReturnAndContainerMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_EnumParameterReturn"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum ECoverageUFunctionState
			{
				UFunctionStateIdle = 0,
				UFunctionStateReady = 4,
				UFunctionStateArmed = 10,
				UFunctionStateFired = 28
			}

			UCLASS()
			class ACoverageUFunctionEnumActor : AActor
			{
				UPROPERTY()
				ECoverageUFunctionState LastState = ECoverageUFunctionState::UFunctionStateIdle;

				UPROPERTY()
				int LastScore = 0;

				UFUNCTION(BlueprintCallable, Category="Coverage|Enum")
				int EvaluateEnumMatrix(ECoverageUFunctionState State, const TArray<ECoverageUFunctionState>&in States)
				{
					LastState = State;

					int Score = int(State);
					for (int Index = 0; Index < States.Num(); ++Index)
					{
						Score += int(States[Index]);
					}

					LastScore = Score;
					return Score;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Enum")
				ECoverageUFunctionState ReturnState(bool bUseFired)
				{
					return bUseFired ? ECoverageUFunctionState::UFunctionStateFired : ECoverageUFunctionState::UFunctionStateReady;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Enum")
				TArray<ECoverageUFunctionState> ReturnStateArray()
				{
					TArray<ECoverageUFunctionState> States;
					States.Add(ECoverageUFunctionState::UFunctionStateReady);
					States.Add(ECoverageUFunctionState::UFunctionStateFired);
					return States;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Enum")
				TMap<ECoverageUFunctionState, int> ReturnStateMap()
				{
					TMap<ECoverageUFunctionState, int> Scores;
					Scores.Add(ECoverageUFunctionState::UFunctionStateArmed, 14);
					Scores.Add(ECoverageUFunctionState::UFunctionStateFired, 28);
					return Scores;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Enum")
				TSet<ECoverageUFunctionState> ReturnStateSet()
				{
					TSet<ECoverageUFunctionState> States;
					States.Add(ECoverageUFunctionState::UFunctionStateReady);
					States.Add(ECoverageUFunctionState::UFunctionStateFired);
					return States;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionEnumParameterReturn.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionEnumActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("enum UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* EvaluateEnumMatrix = FindFunctionForTest(ScriptClass, TEXT("EvaluateEnumMatrix"));
		UFunction* ReturnState = FindFunctionForTest(ScriptClass, TEXT("ReturnState"));
		UFunction* ReturnStateArray = FindFunctionForTest(ScriptClass, TEXT("ReturnStateArray"));
		UFunction* ReturnStateMap = FindFunctionForTest(ScriptClass, TEXT("ReturnStateMap"));
		UFunction* ReturnStateSet = FindFunctionForTest(ScriptClass, TEXT("ReturnStateSet"));
		ASSERT_THAT(IsNotNull(EvaluateEnumMatrix, TEXT("enum matrix UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnState, TEXT("enum return UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnStateArray, TEXT("TArray<UENUM> return UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnStateMap, TEXT("TMap<UENUM,int> return UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnStateSet, TEXT("TSet<UENUM> return UFUNCTION should be generated")));
		if (EvaluateEnumMatrix == nullptr || ReturnState == nullptr || ReturnStateArray == nullptr || ReturnStateMap == nullptr || ReturnStateSet == nullptr)
		{
			return;
		}

		FEnumProperty* StateParam = CastField<FEnumProperty>(FindParameterForTest(EvaluateEnumMatrix, TEXT("State")));
		FArrayProperty* StatesParam = CastField<FArrayProperty>(FindParameterForTest(EvaluateEnumMatrix, TEXT("States")));
		FIntProperty* ScoreReturn = CastField<FIntProperty>(FindParameterForTest(EvaluateEnumMatrix, TEXT("ReturnValue")));
		FEnumProperty* StateReturn = CastField<FEnumProperty>(FindParameterForTest(ReturnState, TEXT("ReturnValue")));
		FArrayProperty* ArrayReturn = CastField<FArrayProperty>(FindParameterForTest(ReturnStateArray, TEXT("ReturnValue")));
		FMapProperty* MapReturn = CastField<FMapProperty>(FindParameterForTest(ReturnStateMap, TEXT("ReturnValue")));
		FSetProperty* SetReturn = CastField<FSetProperty>(FindParameterForTest(ReturnStateSet, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(StateParam, TEXT("UENUM parameter should reflect as FEnumProperty")));
		ASSERT_THAT(IsNotNull(StatesParam, TEXT("TArray<UENUM> parameter should reflect as FArrayProperty")));
		ASSERT_THAT(IsNotNull(ScoreReturn, TEXT("enum matrix return should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(StateReturn, TEXT("UENUM return should reflect as FEnumProperty")));
		ASSERT_THAT(IsNotNull(ArrayReturn, TEXT("TArray<UENUM> return should reflect as FArrayProperty")));
		ASSERT_THAT(IsNotNull(MapReturn, TEXT("TMap<UENUM,int> return should reflect as FMapProperty")));
		ASSERT_THAT(IsNotNull(SetReturn, TEXT("TSet<UENUM> return should reflect as FSetProperty")));
		if (StateParam == nullptr || StatesParam == nullptr || ScoreReturn == nullptr || StateReturn == nullptr
			|| ArrayReturn == nullptr || MapReturn == nullptr || SetReturn == nullptr)
		{
			return;
		}

		FEnumProperty* StatesInner = CastField<FEnumProperty>(StatesParam->Inner);
		FEnumProperty* ArrayInner = CastField<FEnumProperty>(ArrayReturn->Inner);
		FEnumProperty* MapKey = CastField<FEnumProperty>(MapReturn->KeyProp);
		FIntProperty* MapValue = CastField<FIntProperty>(MapReturn->ValueProp);
		FEnumProperty* SetElement = CastField<FEnumProperty>(SetReturn->ElementProp);
		ASSERT_THAT(IsNotNull(StatesInner, TEXT("TArray<UENUM> parameter inner should be FEnumProperty")));
		ASSERT_THAT(IsNotNull(ArrayInner, TEXT("TArray<UENUM> return inner should be FEnumProperty")));
		ASSERT_THAT(IsNotNull(MapKey, TEXT("TMap<UENUM,int> return key should be FEnumProperty")));
		ASSERT_THAT(IsNotNull(MapValue, TEXT("TMap<UENUM,int> return value should be FIntProperty")));
		ASSERT_THAT(IsNotNull(SetElement, TEXT("TSet<UENUM> return element should be FEnumProperty")));
		if (StatesInner == nullptr || ArrayInner == nullptr || MapKey == nullptr || MapValue == nullptr || SetElement == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(StateParam->GetEnum(), StatesInner->GetEnum(),
			TEXT("UENUM scalar and array parameter should reference the same generated enum")));
		ASSERT_THAT(AreEqual(StateParam->GetEnum(), StateReturn->GetEnum(),
			TEXT("UENUM return should reference the same generated enum")));
		ASSERT_THAT(AreEqual(StateParam->GetEnum(), MapKey->GetEnum(),
			TEXT("TMap<UENUM,int> key should reference the same generated enum")));
		ASSERT_THAT(IsTrue(StatesParam->HasAllPropertyFlags(CPF_ConstParm | CPF_OutParm | CPF_ReferenceParm),
			TEXT("const TArray<UENUM> &in should carry const/out/reference flags")));
		ASSERT_THAT(IsTrue(ArrayReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("TArray<UENUM> return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(MapReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("TMap<UENUM,int> return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(SetReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("TSet<UENUM> return should carry CPF_ReturnParm")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("enum UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker EvaluateInvoker(*TestRunner, Actor, TEXT("EvaluateEnumMatrix"));
		ASSERT_THAT(IsTrue(EvaluateInvoker.IsValid(), TEXT("EvaluateEnumMatrix should be invokable")));
		if (!EvaluateInvoker.IsValid())
		{
			return;
		}

		FProperty* ParamProperty = nullptr;
		void* ParamSlot = nullptr;
		ASSERT_THAT(IsTrue(EvaluateInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("EvaluateEnumMatrix should expose scalar enum slot")));
		FEnumProperty* RuntimeStateParam = CastField<FEnumProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(RuntimeStateParam, TEXT("scalar enum slot should be FEnumProperty")));
		if (ParamSlot == nullptr || RuntimeStateParam == nullptr)
		{
			return;
		}
		RuntimeStateParam->GetUnderlyingProperty()->SetIntPropertyValue(ParamSlot, static_cast<int64>(4));

		ASSERT_THAT(IsTrue(EvaluateInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("EvaluateEnumMatrix should expose array enum slot")));
		FArrayProperty* RuntimeStatesParam = CastField<FArrayProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(RuntimeStatesParam, TEXT("array enum slot should be FArrayProperty")));
		if (ParamSlot == nullptr || RuntimeStatesParam == nullptr)
		{
			return;
		}
		FEnumProperty* RuntimeStatesInner = CastField<FEnumProperty>(RuntimeStatesParam->Inner);
		ASSERT_THAT(IsNotNull(RuntimeStatesInner, TEXT("array enum slot inner should be FEnumProperty")));
		if (RuntimeStatesInner == nullptr)
		{
			return;
		}
		FScriptArrayHelper RuntimeStatesHelper(RuntimeStatesParam, ParamSlot);
		const int32 FirstIndex = RuntimeStatesHelper.AddValue();
		RuntimeStatesInner->GetUnderlyingProperty()->SetIntPropertyValue(RuntimeStatesHelper.GetRawPtr(FirstIndex), static_cast<int64>(10));
		const int32 SecondIndex = RuntimeStatesHelper.AddValue();
		RuntimeStatesInner->GetUnderlyingProperty()->SetIntPropertyValue(RuntimeStatesHelper.GetRawPtr(SecondIndex), static_cast<int64>(28));
		ASSERT_THAT(AreEqual(42, EvaluateInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("UENUM scalar plus TArray<UENUM> parameter should execute through reflection")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastScore"), 42,
			TEXT("UENUM UFUNCTION call should update reflected score"))));

		FFunctionInvoker ReturnStateInvoker(*TestRunner, Actor, TEXT("ReturnState"));
		ASSERT_THAT(IsTrue(ReturnStateInvoker.IsValid(), TEXT("ReturnState should be invokable")));
		if (!ReturnStateInvoker.IsValid())
		{
			return;
		}
		ReturnStateInvoker.AddParam<bool>(true);
		ASSERT_THAT(IsTrue(ReturnStateInvoker.Call(), TEXT("ReturnState should execute")));
		const void* StateReturnSlot = StateReturn->ContainerPtrToValuePtr<void>(ReturnStateInvoker.GetParamsMemory());
		ASSERT_THAT(AreEqual(28LL, StateReturn->GetUnderlyingProperty()->GetSignedIntPropertyValue(StateReturnSlot),
			TEXT("UENUM return should preserve the selected enumerator")));

		FFunctionInvoker ReturnArrayInvoker(*TestRunner, Actor, TEXT("ReturnStateArray"));
		ASSERT_THAT(IsTrue(ReturnArrayInvoker.IsValid(), TEXT("ReturnStateArray should be invokable")));
		if (!ReturnArrayInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ReturnArrayInvoker.Call(), TEXT("ReturnStateArray should execute")));
		void* ReturnSlot = ArrayReturn->ContainerPtrToValuePtr<void>(ReturnArrayInvoker.GetParamsMemory());
		FScriptArrayHelper ReturnArrayHelper(ArrayReturn, ReturnSlot);
		ASSERT_THAT(AreEqual(2, ReturnArrayHelper.Num(), TEXT("TArray<UENUM> return should contain two entries")));
		if (ReturnArrayHelper.Num() < 2)
		{
			return;
		}
		const int64 ReturnedArrayScore =
			ArrayInner->GetUnderlyingProperty()->GetSignedIntPropertyValue(ReturnArrayHelper.GetRawPtr(0))
			+ ArrayInner->GetUnderlyingProperty()->GetSignedIntPropertyValue(ReturnArrayHelper.GetRawPtr(1));
		ASSERT_THAT(AreEqual(32LL, ReturnedArrayScore,
			TEXT("TArray<UENUM> return should preserve enum element values")));

		FFunctionInvoker ReturnMapInvoker(*TestRunner, Actor, TEXT("ReturnStateMap"));
		ASSERT_THAT(IsTrue(ReturnMapInvoker.IsValid(), TEXT("ReturnStateMap should be invokable")));
		if (!ReturnMapInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ReturnMapInvoker.Call(), TEXT("ReturnStateMap should execute")));
		ReturnSlot = MapReturn->ContainerPtrToValuePtr<void>(ReturnMapInvoker.GetParamsMemory());
		FScriptMapHelper ReturnMapHelper(MapReturn, ReturnSlot);
		ASSERT_THAT(AreEqual(2, ReturnMapHelper.Num(), TEXT("TMap<UENUM,int> return should contain two entries")));
		bool bFoundFiredScore = false;
		for (int32 SparseIndex = 0; SparseIndex < ReturnMapHelper.GetMaxIndex(); ++SparseIndex)
		{
			if (!ReturnMapHelper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			const int64 KeyValue = MapKey->GetUnderlyingProperty()->GetSignedIntPropertyValue(ReturnMapHelper.GetKeyPtr(SparseIndex));
			const int32 Value = MapValue->GetPropertyValue(ReturnMapHelper.GetValuePtr(SparseIndex));
			bFoundFiredScore |= KeyValue == 28 && Value == 28;
		}
		ASSERT_THAT(IsTrue(bFoundFiredScore,
			TEXT("TMap<UENUM,int> return should preserve enum keys and int values")));

		FFunctionInvoker ReturnSetInvoker(*TestRunner, Actor, TEXT("ReturnStateSet"));
		ASSERT_THAT(IsTrue(ReturnSetInvoker.IsValid(), TEXT("ReturnStateSet should be invokable")));
		if (!ReturnSetInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ReturnSetInvoker.Call(), TEXT("ReturnStateSet should execute")));
		ReturnSlot = SetReturn->ContainerPtrToValuePtr<void>(ReturnSetInvoker.GetParamsMemory());
		FScriptSetHelper ReturnSetHelper(SetReturn, ReturnSlot);
		ASSERT_THAT(AreEqual(2, ReturnSetHelper.Num(), TEXT("TSet<UENUM> return should contain two entries")));
		bool bFoundReady = false;
		bool bFoundFired = false;
		for (int32 SparseIndex = 0; SparseIndex < ReturnSetHelper.GetMaxIndex(); ++SparseIndex)
		{
			if (!ReturnSetHelper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			const int64 ElementValue = SetElement->GetUnderlyingProperty()->GetSignedIntPropertyValue(ReturnSetHelper.GetElementPtr(SparseIndex));
			bFoundReady |= ElementValue == 4;
			bFoundFired |= ElementValue == 28;
		}
		ASSERT_THAT(IsTrue(bFoundReady && bFoundFired,
			TEXT("TSet<UENUM> return should preserve enum element values")));
	}

	TEST_METHOD(HandleParameterAndReturnReflectionMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_HandleParameterReturn"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionHandleActor : AActor
			{
				UPROPERTY()
				TSoftObjectPtr<UObject> LastSoftObject;

				UPROPERTY()
				TSoftClassPtr<AActor> LastSoftClass;

				UPROPERTY()
				TWeakObjectPtr<AActor> LastWeakActor;

				UPROPERTY()
				bool bWeakWasValid = false;

				UFUNCTION(BlueprintCallable, Category="Coverage|Handles")
				int AcceptHandleMatrix(TSoftObjectPtr<UObject> SoftObject, TSoftClassPtr<AActor> SoftClass, TWeakObjectPtr<AActor> WeakActor)
				{
					LastSoftObject = SoftObject;
					LastSoftClass = SoftClass;
					LastWeakActor = WeakActor;
					bWeakWasValid = WeakActor.IsValid() && WeakActor.Get() == this;

					int Score = 0;
					if (SoftObject.ToString().Contains("DefaultTexture"))
					{
						Score += 1;
					}
					if (SoftClass.ToString().Contains("Actor"))
					{
						Score += 2;
					}
					if (bWeakWasValid)
					{
						Score += 4;
					}
					return Score;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Handles")
				TSoftObjectPtr<UObject> ReturnSoftObject()
				{
					return TSoftObjectPtr<UObject>(FSoftObjectPath("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Handles")
				TSoftClassPtr<AActor> ReturnSoftClass()
				{
					return TSoftClassPtr<AActor>(FSoftObjectPath("/Script/Engine.Actor"));
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Handles")
				TWeakObjectPtr<AActor> ReturnWeakSelf()
				{
					return this;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionHandleParameterReturn.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionHandleActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("handle UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* AcceptHandleMatrix = FindFunctionForTest(ScriptClass, TEXT("AcceptHandleMatrix"));
		UFunction* ReturnSoftObject = FindFunctionForTest(ScriptClass, TEXT("ReturnSoftObject"));
		UFunction* ReturnSoftClass = FindFunctionForTest(ScriptClass, TEXT("ReturnSoftClass"));
		UFunction* ReturnWeakSelf = FindFunctionForTest(ScriptClass, TEXT("ReturnWeakSelf"));
		ASSERT_THAT(IsNotNull(AcceptHandleMatrix, TEXT("handle matrix UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnSoftObject, TEXT("TSoftObjectPtr return UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnSoftClass, TEXT("TSoftClassPtr return UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnWeakSelf, TEXT("TWeakObjectPtr return UFUNCTION should be generated")));
		if (AcceptHandleMatrix == nullptr || ReturnSoftObject == nullptr || ReturnSoftClass == nullptr || ReturnWeakSelf == nullptr)
		{
			return;
		}

		FSoftObjectProperty* SoftObjectParam = CastField<FSoftObjectProperty>(FindParameterForTest(AcceptHandleMatrix, TEXT("SoftObject")));
		FSoftClassProperty* SoftClassParam = CastField<FSoftClassProperty>(FindParameterForTest(AcceptHandleMatrix, TEXT("SoftClass")));
		FWeakObjectProperty* WeakActorParam = CastField<FWeakObjectProperty>(FindParameterForTest(AcceptHandleMatrix, TEXT("WeakActor")));
		FIntProperty* ScoreReturn = CastField<FIntProperty>(FindParameterForTest(AcceptHandleMatrix, TEXT("ReturnValue")));
		FSoftObjectProperty* SoftObjectReturn = CastField<FSoftObjectProperty>(FindParameterForTest(ReturnSoftObject, TEXT("ReturnValue")));
		FSoftClassProperty* SoftClassReturn = CastField<FSoftClassProperty>(FindParameterForTest(ReturnSoftClass, TEXT("ReturnValue")));
		FWeakObjectProperty* WeakSelfReturn = CastField<FWeakObjectProperty>(FindParameterForTest(ReturnWeakSelf, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(SoftObjectParam, TEXT("TSoftObjectPtr<UObject> parameter should reflect as FSoftObjectProperty")));
		ASSERT_THAT(IsNotNull(SoftClassParam, TEXT("TSoftClassPtr<AActor> parameter should reflect as FSoftClassProperty")));
		ASSERT_THAT(IsNotNull(WeakActorParam, TEXT("TWeakObjectPtr<AActor> parameter should reflect as FWeakObjectProperty")));
		ASSERT_THAT(IsNotNull(ScoreReturn, TEXT("handle matrix return should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(SoftObjectReturn, TEXT("TSoftObjectPtr<UObject> return should reflect as FSoftObjectProperty")));
		ASSERT_THAT(IsNotNull(SoftClassReturn, TEXT("TSoftClassPtr<AActor> return should reflect as FSoftClassProperty")));
		ASSERT_THAT(IsNotNull(WeakSelfReturn, TEXT("TWeakObjectPtr<AActor> return should reflect as FWeakObjectProperty")));
		if (SoftObjectParam == nullptr || SoftClassParam == nullptr || WeakActorParam == nullptr || ScoreReturn == nullptr
			|| SoftObjectReturn == nullptr || SoftClassReturn == nullptr || WeakSelfReturn == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(UObject::StaticClass(), SoftObjectParam->PropertyClass,
			TEXT("TSoftObjectPtr<UObject> parameter should preserve UObject class")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), SoftClassParam->MetaClass,
			TEXT("TSoftClassPtr<AActor> parameter should preserve AActor meta class")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), WeakActorParam->PropertyClass,
			TEXT("TWeakObjectPtr<AActor> parameter should preserve AActor class")));
		ASSERT_THAT(IsTrue(SoftObjectReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("TSoftObjectPtr<UObject> return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(SoftClassReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("TSoftClassPtr<AActor> return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(WeakSelfReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("TWeakObjectPtr<AActor> return should carry CPF_ReturnParm")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("handle UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		const FSoftObjectPath ExpectedSoftObjectPath(TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
		const FSoftObjectPath ExpectedSoftClassPath(TEXT("/Script/Engine.Actor"));
		FFunctionInvoker AcceptInvoker(*TestRunner, Actor, TEXT("AcceptHandleMatrix"));
		ASSERT_THAT(IsTrue(AcceptInvoker.IsValid(), TEXT("AcceptHandleMatrix should be invokable")));
		if (!AcceptInvoker.IsValid())
		{
			return;
		}
		AcceptInvoker.AddParam<TSoftObjectPtr<UObject>>(TSoftObjectPtr<UObject>(ExpectedSoftObjectPath));
		FProperty* HandleParamProperty = nullptr;
		void* HandleParamSlot = nullptr;
		ASSERT_THAT(IsTrue(AcceptInvoker.AddParamSlot(HandleParamProperty, HandleParamSlot),
			TEXT("AcceptHandleMatrix should expose TSoftClassPtr parameter slot")));
		FSoftClassProperty* RuntimeSoftClassParam = CastField<FSoftClassProperty>(HandleParamProperty);
		ASSERT_THAT(IsNotNull(RuntimeSoftClassParam, TEXT("runtime TSoftClassPtr slot should be FSoftClassProperty")));
		if (HandleParamSlot == nullptr || RuntimeSoftClassParam == nullptr)
		{
			return;
		}
		*static_cast<FSoftObjectPtr*>(HandleParamSlot) = FSoftObjectPtr(ExpectedSoftClassPath);
		AcceptInvoker.AddParam<TWeakObjectPtr<AActor>>(TWeakObjectPtr<AActor>(Actor));
		ASSERT_THAT(AreEqual(7, AcceptInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("handle UFUNCTION should consume soft object, soft class, and weak object parameters")));

		FSoftObjectPath ActualSoftObjectPath;
		ASSERT_THAT(IsTrue(GetSoftObjectPathByPath(*TestRunner, Actor, TEXT("LastSoftObject"), ActualSoftObjectPath),
			TEXT("LastSoftObject should be readable after handle call")));
		ASSERT_THAT(AreEqual(ExpectedSoftObjectPath, ActualSoftObjectPath,
			TEXT("TSoftObjectPtr parameter should round-trip into script state")));
		FSoftObjectPath ActualSoftClassPath;
		ASSERT_THAT(IsTrue(GetSoftClassPathByPath(*TestRunner, Actor, TEXT("LastSoftClass"), ActualSoftClassPath),
			TEXT("LastSoftClass should be readable after handle call")));
		ASSERT_THAT(AreEqual(ExpectedSoftClassPath, ActualSoftClassPath,
			TEXT("TSoftClassPtr parameter should round-trip into script state")));
		UObject* LastWeakObject = nullptr;
		ASSERT_THAT(IsTrue(GetWeakObjectByPath(*TestRunner, Actor, TEXT("LastWeakActor"), LastWeakObject),
			TEXT("LastWeakActor should be readable after handle call")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), LastWeakObject,
			TEXT("TWeakObjectPtr parameter should resolve back to the actor")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bWeakWasValid"), true,
			TEXT("TWeakObjectPtr parameter should be valid inside script execution"))));

		FFunctionInvoker ReturnSoftObjectInvoker(*TestRunner, Actor, TEXT("ReturnSoftObject"));
		ASSERT_THAT(IsTrue(ReturnSoftObjectInvoker.IsValid(), TEXT("ReturnSoftObject should be invokable")));
		if (!ReturnSoftObjectInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ReturnSoftObjectInvoker.Call(), TEXT("ReturnSoftObject should execute")));
		const FSoftObjectPtr* SoftObjectReturnValue = static_cast<const FSoftObjectPtr*>(SoftObjectReturn->ContainerPtrToValuePtr<void>(ReturnSoftObjectInvoker.GetParamsMemory()));
		ASSERT_THAT(IsNotNull(SoftObjectReturnValue, TEXT("TSoftObjectPtr return slot should be readable")));
		if (SoftObjectReturnValue == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(ExpectedSoftObjectPath, SoftObjectReturnValue->ToSoftObjectPath(),
			TEXT("TSoftObjectPtr return should preserve its soft object path")));

		FFunctionInvoker ReturnSoftClassInvoker(*TestRunner, Actor, TEXT("ReturnSoftClass"));
		ASSERT_THAT(IsTrue(ReturnSoftClassInvoker.IsValid(), TEXT("ReturnSoftClass should be invokable")));
		if (!ReturnSoftClassInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ReturnSoftClassInvoker.Call(), TEXT("ReturnSoftClass should execute")));
		const FSoftObjectPtr* SoftClassReturnValue = static_cast<const FSoftObjectPtr*>(SoftClassReturn->ContainerPtrToValuePtr<void>(ReturnSoftClassInvoker.GetParamsMemory()));
		ASSERT_THAT(IsNotNull(SoftClassReturnValue, TEXT("TSoftClassPtr return slot should be readable")));
		if (SoftClassReturnValue == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(ExpectedSoftClassPath, SoftClassReturnValue->ToSoftObjectPath(),
			TEXT("TSoftClassPtr return should preserve its soft class path")));

		FFunctionInvoker ReturnWeakInvoker(*TestRunner, Actor, TEXT("ReturnWeakSelf"));
		ASSERT_THAT(IsTrue(ReturnWeakInvoker.IsValid(), TEXT("ReturnWeakSelf should be invokable")));
		if (!ReturnWeakInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ReturnWeakInvoker.Call(), TEXT("ReturnWeakSelf should execute")));
		const void* WeakReturnSlot = WeakSelfReturn->ContainerPtrToValuePtr<void>(ReturnWeakInvoker.GetParamsMemory());
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), WeakSelfReturn->GetObjectPropertyValue(WeakReturnSlot),
			TEXT("TWeakObjectPtr return should resolve back to the actor")));
	}

	TEST_METHOD(DelegateParameterReflectionAndRuntimeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_DelegateParameter"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			delegate int FCoverageUFunctionComputeDelegate(int Value, const FString& Label);

			UCLASS()
			class ACoverageUFunctionDelegateActor : AActor
			{
				UPROPERTY()
				int LastDelegateResult = 0;

				UPROPERTY()
				FString LastDelegateLabel;

				UFUNCTION(BlueprintCallable, Category="Coverage|Delegate")
				int AcceptDelegate(FCoverageUFunctionComputeDelegate Callback)
				{
					if (!Callback.IsBound())
					{
						LastDelegateResult = -1;
						return -1;
					}

					LastDelegateResult = Callback.Execute(20, "DelegateLabel");
					return LastDelegateResult;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Delegate")
				int ComputeFromDelegate(int Value, const FString&in Label)
				{
					LastDelegateLabel = Label;
					return Value + Label.Len() + 9;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Delegate")
				int ExerciseDelegateParameter()
				{
					FCoverageUFunctionComputeDelegate Callback;
					Callback.BindUFunction(this, n"ComputeFromDelegate");
					return AcceptDelegate(Callback);
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionDelegateParameter.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionDelegateActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("delegate-parameter UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* AcceptDelegate = FindFunctionForTest(ScriptClass, TEXT("AcceptDelegate"));
		UFunction* ComputeFromDelegate = FindFunctionForTest(ScriptClass, TEXT("ComputeFromDelegate"));
		UFunction* ExerciseDelegateParameter = FindFunctionForTest(ScriptClass, TEXT("ExerciseDelegateParameter"));
		ASSERT_THAT(IsNotNull(AcceptDelegate, TEXT("delegate consumer UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ComputeFromDelegate, TEXT("delegate target UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ExerciseDelegateParameter, TEXT("delegate exercise UFUNCTION should be generated")));
		if (AcceptDelegate == nullptr || ComputeFromDelegate == nullptr || ExerciseDelegateParameter == nullptr)
		{
			return;
		}

		FDelegateProperty* CallbackParam = CastField<FDelegateProperty>(FindParameterForTest(AcceptDelegate, TEXT("Callback")));
		FIntProperty* AcceptReturn = CastField<FIntProperty>(FindParameterForTest(AcceptDelegate, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(CallbackParam, TEXT("delegate UFUNCTION parameter should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(AcceptReturn, TEXT("delegate consumer return should reflect as FIntProperty")));
		if (CallbackParam == nullptr || AcceptReturn == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(CallbackParam->SignatureFunction,
			TEXT("delegate UFUNCTION parameter should keep its generated signature function")));
		if (CallbackParam->SignatureFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(CallbackParam->SignatureFunction, TEXT("Value")),
			TEXT("delegate parameter signature should expose int input")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStrProperty>(CallbackParam->SignatureFunction, TEXT("Label")),
			TEXT("delegate parameter signature should expose FString input")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(CallbackParam->SignatureFunction, TEXT("ReturnValue")),
			TEXT("delegate parameter signature should expose int return")));

		FStrProperty* ComputeLabelParam = CastField<FStrProperty>(FindParameterForTest(ComputeFromDelegate, TEXT("Label")));
		ASSERT_THAT(IsNotNull(ComputeLabelParam,
			TEXT("delegate target FString parameter should reflect as FStrProperty")));
		if (ComputeLabelParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ComputeLabelParam->HasAllPropertyFlags(CPF_ConstParm | CPF_OutParm | CPF_ReferenceParm),
			TEXT("const FString&in delegate target parameter should carry const/out/reference flags")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("delegate-parameter UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker ExerciseInvoker(*TestRunner, Actor, TEXT("ExerciseDelegateParameter"));
		ASSERT_THAT(IsTrue(ExerciseInvoker.IsValid(), TEXT("ExerciseDelegateParameter should be invokable")));
		if (!ExerciseInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(42, ExerciseInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("delegate UFUNCTION parameter should execute a bound script callback")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastDelegateResult"), 42,
			TEXT("delegate UFUNCTION parameter should update result state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastDelegateLabel"), FString(TEXT("DelegateLabel")),
			TEXT("delegate UFUNCTION parameter should pass FString payload to callback"))));
	}

	TEST_METHOD(PrimitiveParameterOrderAndOutLayoutMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_PrimitiveOrderLayout"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum ECoverageUFunctionOrderState
			{
				OrderStateIdle = 0,
				OrderStateActive = 2
			}

			UCLASS()
			class ACoverageUFunctionPrimitiveOrderActor : AActor
			{
				UPROPERTY()
				int LastOutScore = 0;

				UFUNCTION(BlueprintCallable, Category="Coverage|Layout", meta=(DisplayName="Ordered Parameter Matrix", AdvancedDisplay="Ratio,ObjectValue"))
				int ConsumeOrdered(bool bEnabled, uint8 ByteValue, ECoverageUFunctionOrderState State, double Ratio, UObject ObjectValue, int&out OutScore)
				{
					OutScore = (bEnabled ? 1 : 0) + int(ByteValue) + int(State) + int(Ratio) + (ObjectValue != nullptr ? 10 : 0);
					LastOutScore = OutScore;
					return OutScore;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionPrimitiveOrderLayout.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionPrimitiveOrderActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("primitive order UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* ConsumeOrdered = FindFunctionForTest(ScriptClass, TEXT("ConsumeOrdered"));
		ASSERT_THAT(IsNotNull(ConsumeOrdered, TEXT("ordered primitive UFUNCTION should be generated")));
		if (ConsumeOrdered == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("Ordered Parameter Matrix")), ConsumeOrdered->GetMetaData(TEXT("DisplayName")),
			TEXT("ordered UFUNCTION DisplayName metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("Ratio,ObjectValue")), ConsumeOrdered->GetMetaData(TEXT("AdvancedDisplay")),
			TEXT("ordered UFUNCTION AdvancedDisplay metadata should be preserved")));

		const FName ExpectedInputOrder[] = {
			TEXT("bEnabled"),
			TEXT("ByteValue"),
			TEXT("State"),
			TEXT("Ratio"),
			TEXT("ObjectValue"),
			TEXT("OutScore"),
		};
		const int32 ExpectedInputCount = static_cast<int32>(UE_ARRAY_COUNT(ExpectedInputOrder));
		int32 InputIndex = 0;
		for (TFieldIterator<FProperty> It(ConsumeOrdered); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}

			ASSERT_THAT(IsTrue(InputIndex < ExpectedInputCount,
				TEXT("ordered UFUNCTION should not expose extra input parameters")));
			if (InputIndex >= ExpectedInputCount)
			{
				return;
			}
			ASSERT_THAT(AreEqual(ExpectedInputOrder[InputIndex], It->GetFName(),
				TEXT("ordered UFUNCTION input parameter order should match declaration order")));
			++InputIndex;
		}
		ASSERT_THAT(AreEqual(ExpectedInputCount, InputIndex,
			TEXT("ordered UFUNCTION should expose every declared input parameter")));

		FBoolProperty* EnabledParam = CastField<FBoolProperty>(FindParameterForTest(ConsumeOrdered, TEXT("bEnabled")));
		FByteProperty* ByteParam = CastField<FByteProperty>(FindParameterForTest(ConsumeOrdered, TEXT("ByteValue")));
		FEnumProperty* StateParam = CastField<FEnumProperty>(FindParameterForTest(ConsumeOrdered, TEXT("State")));
		FDoubleProperty* RatioParam = CastField<FDoubleProperty>(FindParameterForTest(ConsumeOrdered, TEXT("Ratio")));
		FObjectProperty* ObjectParam = CastField<FObjectProperty>(FindParameterForTest(ConsumeOrdered, TEXT("ObjectValue")));
		FIntProperty* OutScoreParam = CastField<FIntProperty>(FindParameterForTest(ConsumeOrdered, TEXT("OutScore")));
		FIntProperty* ReturnValue = CastField<FIntProperty>(FindParameterForTest(ConsumeOrdered, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(EnabledParam, TEXT("bool parameter should reflect as FBoolProperty")));
		ASSERT_THAT(IsNotNull(ByteParam, TEXT("uint8 parameter should reflect as FByteProperty")));
		ASSERT_THAT(IsNotNull(StateParam, TEXT("UENUM parameter should reflect as FEnumProperty")));
		ASSERT_THAT(IsNotNull(RatioParam, TEXT("double parameter should reflect as FDoubleProperty")));
		ASSERT_THAT(IsNotNull(ObjectParam, TEXT("UObject parameter should reflect as FObjectProperty")));
		ASSERT_THAT(IsNotNull(OutScoreParam, TEXT("int &out parameter should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(ReturnValue, TEXT("ordered UFUNCTION return should reflect as FIntProperty")));
		if (EnabledParam == nullptr || ByteParam == nullptr || StateParam == nullptr || RatioParam == nullptr
			|| ObjectParam == nullptr || OutScoreParam == nullptr || ReturnValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(UObject::StaticClass(), ObjectParam->PropertyClass,
			TEXT("UObject parameter should preserve UObject class")));
		ASSERT_THAT(IsTrue(OutScoreParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("ordered int &out parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(OutScoreParam->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReturnParm),
			TEXT("ordered int &out parameter should not carry const or return flags")));
		ASSERT_THAT(IsTrue(ReturnValue->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("ordered UFUNCTION return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(ConsumeOrdered->HasAnyFunctionFlags(FUNC_HasOutParms),
			TEXT("ordered UFUNCTION with out parameter and return should set FUNC_HasOutParms")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("primitive order UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("ConsumeOrdered"));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ConsumeOrdered should be invokable")));
		if (!Invoker.IsValid())
		{
			return;
		}
		Invoker.AddParam<bool>(true);
		Invoker.AddParam<uint8>(4);
		FProperty* ParamProperty = nullptr;
		void* ParamSlot = nullptr;
		ASSERT_THAT(IsTrue(Invoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("ConsumeOrdered should expose enum parameter slot")));
		FEnumProperty* RuntimeStateParam = CastField<FEnumProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(RuntimeStateParam, TEXT("runtime enum slot should be FEnumProperty")));
		if (ParamSlot == nullptr || RuntimeStateParam == nullptr)
		{
			return;
		}
		RuntimeStateParam->GetUnderlyingProperty()->SetIntPropertyValue(ParamSlot, static_cast<int64>(2));
		Invoker.AddParam<double>(25.0);
		Invoker.AddParam<UObject*>(Actor);
		FProperty* OutProperty = nullptr;
		void* OutSlot = nullptr;
		ASSERT_THAT(IsTrue(Invoker.AddParamSlot(OutProperty, OutSlot),
			TEXT("ConsumeOrdered should expose out parameter slot")));
		FIntProperty* RuntimeOutParam = CastField<FIntProperty>(OutProperty);
		ASSERT_THAT(IsNotNull(RuntimeOutParam, TEXT("runtime out slot should be FIntProperty")));
		if (OutSlot == nullptr || RuntimeOutParam == nullptr)
		{
			return;
		}
		RuntimeOutParam->SetPropertyValue(OutSlot, 0);
		ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("ordered primitive UFUNCTION should execute through reflection")));
		ASSERT_THAT(AreEqual(42, RuntimeOutParam->GetPropertyValue(OutSlot),
			TEXT("ordered int &out parameter should write caller buffer")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastOutScore"), 42,
			TEXT("ordered UFUNCTION should update script state"))));
	}

	TEST_METHOD(DefaultArgumentsAndOutFlagLayout)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_DefaultOutLayout"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionDefaultOutActor : AActor
			{
				UPROPERTY()
				int LastInput = 0;

				UPROPERTY()
				int LastOutput = 0;

				UFUNCTION(BlueprintCallable, Category="Coverage|Defaults")
				int AddDefaults(int Base, int Delta = 7, int Extra = 3)
				{
					LastInput = Base + Delta + Extra;
					return LastInput;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Out")
				void WriteOutput(int Input, int&out Output)
				{
					Output = Input + 5;
					LastOutput = Output;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionDefaultOutLayout.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionDefaultOutActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("default/out UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* AddDefaults = FindFunctionForTest(ScriptClass, TEXT("AddDefaults"));
		UFunction* WriteOutput = FindFunctionForTest(ScriptClass, TEXT("WriteOutput"));
		ASSERT_THAT(IsNotNull(AddDefaults, TEXT("default argument UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(WriteOutput, TEXT("out parameter UFUNCTION should be generated")));
		if (AddDefaults == nullptr || WriteOutput == nullptr)
		{
			return;
		}

		FIntProperty* BaseParam = CastField<FIntProperty>(FindParameterForTest(AddDefaults, TEXT("Base")));
		FIntProperty* DeltaParam = CastField<FIntProperty>(FindParameterForTest(AddDefaults, TEXT("Delta")));
		FIntProperty* ExtraParam = CastField<FIntProperty>(FindParameterForTest(AddDefaults, TEXT("Extra")));
		FIntProperty* AddReturnValue = CastField<FIntProperty>(FindParameterForTest(AddDefaults, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(BaseParam, TEXT("Base default-matrix parameter should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(DeltaParam, TEXT("Delta default-matrix parameter should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(ExtraParam, TEXT("Extra default-matrix parameter should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(AddReturnValue, TEXT("AddDefaults return value should reflect as FIntProperty")));
		if (BaseParam == nullptr || DeltaParam == nullptr || ExtraParam == nullptr || AddReturnValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(BaseParam->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm),
			TEXT("non-default input parameter should be a plain input parameter")));
		ASSERT_THAT(AreEqual(FString(TEXT("7")), AddDefaults->GetMetaData(TEXT("CPP_Default_Delta")),
			TEXT("Delta default argument should round-trip through CPP_Default metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("3")), AddDefaults->GetMetaData(TEXT("CPP_Default_Extra")),
			TEXT("Extra default argument should round-trip through CPP_Default metadata")));
		ASSERT_THAT(IsTrue(AddReturnValue->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("default argument UFUNCTION return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(AddDefaults->HasAnyFunctionFlags(FUNC_HasOutParms),
			TEXT("returning UFUNCTION should set FUNC_HasOutParms for the return slot")));

		FIntProperty* InputParam = CastField<FIntProperty>(FindParameterForTest(WriteOutput, TEXT("Input")));
		FIntProperty* OutputParam = CastField<FIntProperty>(FindParameterForTest(WriteOutput, TEXT("Output")));
		ASSERT_THAT(IsNotNull(InputParam, TEXT("WriteOutput input should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(OutputParam, TEXT("WriteOutput output should reflect as FIntProperty")));
		if (InputParam == nullptr || OutputParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(InputParam->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm),
			TEXT("WriteOutput input should remain a plain input parameter")));
		ASSERT_THAT(IsTrue(OutputParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("int &out parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(OutputParam->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReturnParm),
			TEXT("int &out parameter should not carry const or return flags")));
		ASSERT_THAT(IsTrue(WriteOutput->HasAnyFunctionFlags(FUNC_HasOutParms),
			TEXT("out parameter UFUNCTION should set FUNC_HasOutParms")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("default/out UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker AddInvoker(*TestRunner, Actor, TEXT("AddDefaults"));
		ASSERT_THAT(IsTrue(AddInvoker.IsValid(), TEXT("AddDefaults should be invokable through reflection")));
		if (!AddInvoker.IsValid())
		{
			return;
		}
		AddInvoker.AddParam<int32>(20);
		AddInvoker.AddParam<int32>(11);
		AddInvoker.AddParam<int32>(11);
		ASSERT_THAT(AreEqual(42, AddInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("default argument UFUNCTION should execute with reflected explicit parameters")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastInput"), 42,
			TEXT("default argument UFUNCTION should update reflected state"))));

		FFunctionInvoker OutInvoker(*TestRunner, Actor, TEXT("WriteOutput"));
		ASSERT_THAT(IsTrue(OutInvoker.IsValid(), TEXT("WriteOutput should be invokable through reflection")));
		if (!OutInvoker.IsValid())
		{
			return;
		}
		OutInvoker.AddParam<int32>(37);
		FProperty* OutputSlotProperty = nullptr;
		void* OutputSlot = nullptr;
		ASSERT_THAT(IsTrue(OutInvoker.AddParamSlot(OutputSlotProperty, OutputSlot),
			TEXT("WriteOutput should expose the int &out parameter slot")));
		FIntProperty* OutputSlotIntProperty = CastField<FIntProperty>(OutputSlotProperty);
		ASSERT_THAT(IsNotNull(OutputSlotIntProperty, TEXT("WriteOutput out slot should be an FIntProperty")));
		if (OutputSlot == nullptr || OutputSlotIntProperty == nullptr)
		{
			return;
		}
		OutputSlotIntProperty->SetPropertyValue(OutputSlot, 0);
		ASSERT_THAT(IsTrue(OutInvoker.Call(), TEXT("WriteOutput should write the reflected out parameter")));
		ASSERT_THAT(AreEqual(42, OutputSlotIntProperty->GetPropertyValue(OutputSlot),
			TEXT("int &out value should be written into caller buffer")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastOutput"), 42,
			TEXT("out parameter UFUNCTION should update reflected state"))));
	}

	TEST_METHOD(DefaultArgumentTypeConversionMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_DefaultTypeConversion"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum EUFunctionDefaultChoice
			{
				First = 1,
				Second = 2,
				Third = 3,
			}

			UCLASS()
			class ACoverageUFunctionDefaultTypeActor : AActor
			{
				UPROPERTY()
				FString LastSummary;

				UPROPERTY()
				int LastScore = 0;

				UFUNCTION(BlueprintCallable, Category="Coverage|Defaults")
				int ApplyDefaultTypeMatrix(bool bEnabled = true, FName NameValue = n"DefaultName", FString Label = "DefaultLabel", EUFunctionDefaultChoice Choice = EUFunctionDefaultChoice::Second, UObject ObjectValue = nullptr, TSubclassOf<AActor> ActorClass = nullptr)
				{
					LastScore = (bEnabled ? 1 : 0)
						+ NameValue.ToString().Len()
						+ Label.Len()
						+ int(Choice)
						+ (ObjectValue != nullptr ? 10 : 0)
						+ (ActorClass != nullptr ? 20 : 0);
					LastSummary = NameValue.ToString() + ":" + Label;
					return LastScore;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Defaults")
				int ApplyStructDefaultMatrix(FVector Location = FVector(1.0, 2.0, 3.0), FLinearColor Color = FLinearColor::Red)
				{
					LastScore = int(Location.X + Location.Y + Location.Z + Color.R + Color.G + Color.B + Color.A);
					return LastScore;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionDefaultTypeConversion.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionDefaultTypeActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("default type conversion UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* ApplyDefaultTypeMatrix = FindFunctionForTest(ScriptClass, TEXT("ApplyDefaultTypeMatrix"));
		UFunction* ApplyStructDefaultMatrix = FindFunctionForTest(ScriptClass, TEXT("ApplyStructDefaultMatrix"));
		ASSERT_THAT(IsNotNull(ApplyDefaultTypeMatrix, TEXT("default type conversion UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ApplyStructDefaultMatrix, TEXT("struct default conversion UFUNCTION should be generated")));
		if (ApplyDefaultTypeMatrix == nullptr || ApplyStructDefaultMatrix == nullptr)
		{
			return;
		}

		FBoolProperty* EnabledParam = CastField<FBoolProperty>(FindParameterForTest(ApplyDefaultTypeMatrix, TEXT("bEnabled")));
		FNameProperty* NameParam = CastField<FNameProperty>(FindParameterForTest(ApplyDefaultTypeMatrix, TEXT("NameValue")));
		FStrProperty* LabelParam = CastField<FStrProperty>(FindParameterForTest(ApplyDefaultTypeMatrix, TEXT("Label")));
		FEnumProperty* ChoiceParam = CastField<FEnumProperty>(FindParameterForTest(ApplyDefaultTypeMatrix, TEXT("Choice")));
		FObjectProperty* ObjectParam = CastField<FObjectProperty>(FindParameterForTest(ApplyDefaultTypeMatrix, TEXT("ObjectValue")));
		FClassProperty* ActorClassParam = CastField<FClassProperty>(FindParameterForTest(ApplyDefaultTypeMatrix, TEXT("ActorClass")));
		FIntProperty* DefaultReturnParam = CastField<FIntProperty>(FindParameterForTest(ApplyDefaultTypeMatrix, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(EnabledParam, TEXT("bool default parameter should reflect as FBoolProperty")));
		ASSERT_THAT(IsNotNull(NameParam, TEXT("FName default parameter should reflect as FNameProperty")));
		ASSERT_THAT(IsNotNull(LabelParam, TEXT("FString default parameter should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(ChoiceParam, TEXT("UENUM default parameter should reflect as FEnumProperty")));
		ASSERT_THAT(IsNotNull(ObjectParam, TEXT("UObject default parameter should reflect as FObjectProperty")));
		ASSERT_THAT(IsNotNull(ActorClassParam, TEXT("TSubclassOf default parameter should reflect as FClassProperty")));
		ASSERT_THAT(IsNotNull(DefaultReturnParam, TEXT("default type matrix return should reflect as FIntProperty")));
		if (EnabledParam == nullptr || NameParam == nullptr || LabelParam == nullptr || ChoiceParam == nullptr
			|| ObjectParam == nullptr || ActorClassParam == nullptr || DefaultReturnParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("true")), ApplyDefaultTypeMatrix->GetMetaData(TEXT("CPP_Default_bEnabled")),
			TEXT("bool default should round-trip through CPP_Default metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("DefaultName")), ApplyDefaultTypeMatrix->GetMetaData(TEXT("CPP_Default_NameValue")),
			TEXT("FName literal default should convert to Unreal metadata form")));
		ASSERT_THAT(AreEqual(FString(TEXT("DefaultLabel")), ApplyDefaultTypeMatrix->GetMetaData(TEXT("CPP_Default_Label")),
			TEXT("FString default should trim quotes in CPP_Default metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Second")), ApplyDefaultTypeMatrix->GetMetaData(TEXT("CPP_Default_Choice")),
			TEXT("enum default should strip enum scope in CPP_Default metadata")));
		ASSERT_THAT(AreEqual(FString(), ApplyDefaultTypeMatrix->GetMetaData(TEXT("CPP_Default_ObjectValue")),
			TEXT("nullptr UObject default should convert to an empty Unreal object default")));
		ASSERT_THAT(AreEqual(FString(), ApplyDefaultTypeMatrix->GetMetaData(TEXT("CPP_Default_ActorClass")),
			TEXT("nullptr TSubclassOf default should convert to an empty Unreal class default")));
		ASSERT_THAT(AreEqual(UObject::StaticClass(), ObjectParam->PropertyClass,
			TEXT("default UObject parameter should preserve UObject class")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), ActorClassParam->MetaClass,
			TEXT("default TSubclassOf parameter should preserve AActor metaclass")));
		ASSERT_THAT(IsTrue(DefaultReturnParam->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("default type matrix return should carry CPF_ReturnParm")));

		FStructProperty* LocationParam = CastField<FStructProperty>(FindParameterForTest(ApplyStructDefaultMatrix, TEXT("Location")));
		FStructProperty* ColorParam = CastField<FStructProperty>(FindParameterForTest(ApplyStructDefaultMatrix, TEXT("Color")));
		FIntProperty* StructDefaultReturnParam = CastField<FIntProperty>(FindParameterForTest(ApplyStructDefaultMatrix, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(LocationParam, TEXT("FVector default parameter should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(ColorParam, TEXT("FLinearColor default parameter should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(StructDefaultReturnParam, TEXT("struct default matrix return should reflect as FIntProperty")));
		if (LocationParam == nullptr || ColorParam == nullptr || StructDefaultReturnParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(TBaseStructure<FVector>::Get(), LocationParam->Struct,
			TEXT("FVector default parameter should preserve native struct type")));
		ASSERT_THAT(AreEqual(TBaseStructure<FLinearColor>::Get(), ColorParam->Struct,
			TEXT("FLinearColor default parameter should preserve native struct type")));
		ASSERT_THAT(AreEqual(FString(TEXT("1.000000,2.000000,3.000000")), ApplyStructDefaultMatrix->GetMetaData(TEXT("CPP_Default_Location")),
			TEXT("FVector constructor default should convert to Unreal vector metadata")));
		ASSERT_THAT(AreEqual(FLinearColor::Red.ToString(), ApplyStructDefaultMatrix->GetMetaData(TEXT("CPP_Default_Color")),
			TEXT("FLinearColor named constant default should convert to Unreal color metadata")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("default type conversion actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker DefaultInvoker(*TestRunner, Actor, TEXT("ApplyDefaultTypeMatrix"));
		ASSERT_THAT(IsTrue(DefaultInvoker.IsValid(), TEXT("ApplyDefaultTypeMatrix should be invokable")));
		if (!DefaultInvoker.IsValid())
		{
			return;
		}
		DefaultInvoker.AddParam<bool>(false);
		DefaultInvoker.AddParam<FName>(FName(TEXT("RuntimeName")));
		DefaultInvoker.AddParam<FString>(FString(TEXT("RuntimeLabel")));
		FProperty* ChoiceSlotProperty = nullptr;
		void* ChoiceSlot = nullptr;
		ASSERT_THAT(IsTrue(DefaultInvoker.AddParamSlot(ChoiceSlotProperty, ChoiceSlot),
			TEXT("ApplyDefaultTypeMatrix should expose enum parameter slot")));
		FEnumProperty* RuntimeChoiceParam = CastField<FEnumProperty>(ChoiceSlotProperty);
		ASSERT_THAT(IsNotNull(RuntimeChoiceParam, TEXT("runtime enum default slot should be FEnumProperty")));
		if (ChoiceSlot == nullptr || RuntimeChoiceParam == nullptr)
		{
			return;
		}
		RuntimeChoiceParam->GetUnderlyingProperty()->SetIntPropertyValue(ChoiceSlot, static_cast<int64>(3));
		DefaultInvoker.AddParam<UObject*>(Actor);
		DefaultInvoker.AddParam<TSubclassOf<AActor>>(TSubclassOf<AActor>(ScriptClass));
		ASSERT_THAT(AreEqual(56, DefaultInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("default type matrix should execute with explicit runtime values")));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastSummary"), FString(TEXT("RuntimeName:RuntimeLabel")),
			TEXT("default type matrix should update string summary state"))));

		FFunctionInvoker StructDefaultInvoker(*TestRunner, Actor, TEXT("ApplyStructDefaultMatrix"));
		ASSERT_THAT(IsTrue(StructDefaultInvoker.IsValid(), TEXT("ApplyStructDefaultMatrix should be invokable")));
		if (!StructDefaultInvoker.IsValid())
		{
			return;
		}
		StructDefaultInvoker.AddParam<FVector>(FVector(10.0, 20.0, 5.0));
		StructDefaultInvoker.AddParam<FLinearColor>(FLinearColor(1.0f, 2.0f, 3.0f, 1.0f));
		ASSERT_THAT(AreEqual(42, StructDefaultInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("struct default matrix should execute with explicit FVector and FLinearColor values")));
	}

	TEST_METHOD(ReferenceDirectionFlagAndRuntimeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_ReferenceDirectionMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionReferenceDirectionActor : AActor
			{
				UPROPERTY()
				int LastReadScore = 0;

				UPROPERTY()
				bool bInoutSawOriginal = false;

				UPROPERTY()
				FString LastLabel;

				UPROPERTY()
				FVector LastVector = FVector::ZeroVector;

				UFUNCTION(BlueprintCallable, Category="Coverage|References")
				int ReadConstRefs(const int&in Count, const FString&in Label, const FVector&in Location)
				{
					LastReadScore = Count + Label.Len() + int(Location.X);
					LastLabel = Label;
					LastVector = Location;
					return LastReadScore;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|References")
				void FillOutRefs(int&out Count, FString&out Label, FVector&out Location)
				{
					Count = 42;
					Label = "OutLabel";
					Location = FVector(4.0, 5.0, 6.0);
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|References")
				int MutateInoutRefs(int&inout Count, FString&inout Label, FVector&inout Location)
				{
					bInoutSawOriginal = Count == 10 && Label == "InLabel" && Location.X == 1.0;
					Count += 5;
					Label += "|Mutated";
					Location += FVector(2.0, 3.0, 4.0);
					return Count + Label.Len() + int(Location.Z);
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionReferenceDirectionMatrix.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionReferenceDirectionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("reference-direction UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* ReadConstRefs = FindFunctionForTest(ScriptClass, TEXT("ReadConstRefs"));
		UFunction* FillOutRefs = FindFunctionForTest(ScriptClass, TEXT("FillOutRefs"));
		UFunction* MutateInoutRefs = FindFunctionForTest(ScriptClass, TEXT("MutateInoutRefs"));
		ASSERT_THAT(IsNotNull(ReadConstRefs, TEXT("ReadConstRefs should be generated")));
		ASSERT_THAT(IsNotNull(FillOutRefs, TEXT("FillOutRefs should be generated")));
		ASSERT_THAT(IsNotNull(MutateInoutRefs, TEXT("MutateInoutRefs should be generated")));
		if (ReadConstRefs == nullptr || FillOutRefs == nullptr || MutateInoutRefs == nullptr)
		{
			return;
		}

		FIntProperty* ConstCountParam = CastField<FIntProperty>(FindParameterForTest(ReadConstRefs, TEXT("Count")));
		FStrProperty* ConstLabelParam = CastField<FStrProperty>(FindParameterForTest(ReadConstRefs, TEXT("Label")));
		FStructProperty* ConstLocationParam = CastField<FStructProperty>(FindParameterForTest(ReadConstRefs, TEXT("Location")));
		FIntProperty* ConstReturnParam = CastField<FIntProperty>(FindParameterForTest(ReadConstRefs, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(ConstCountParam, TEXT("const int &in should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(ConstLabelParam, TEXT("const FString &in should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(ConstLocationParam, TEXT("const FVector &in should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(ConstReturnParam, TEXT("ReadConstRefs return should reflect as FIntProperty")));
		if (ConstCountParam == nullptr || ConstLabelParam == nullptr || ConstLocationParam == nullptr || ConstReturnParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ConstCountParam->HasAllPropertyFlags(CPF_ConstParm | CPF_OutParm | CPF_ReferenceParm),
			TEXT("const int &in should carry const/out/reference flags")));
		ASSERT_THAT(IsTrue(ConstLabelParam->HasAllPropertyFlags(CPF_ConstParm | CPF_OutParm | CPF_ReferenceParm),
			TEXT("const FString &in should carry const/out/reference flags")));
		ASSERT_THAT(IsTrue(ConstLocationParam->HasAllPropertyFlags(CPF_ConstParm | CPF_OutParm | CPF_ReferenceParm),
			TEXT("const FVector &in should carry const/out/reference flags")));
		ASSERT_THAT(AreEqual(TBaseStructure<FVector>::Get(), ConstLocationParam->Struct,
			TEXT("const FVector &in should preserve the FVector struct type")));
		ASSERT_THAT(IsTrue(ReadConstRefs->HasAnyFunctionFlags(FUNC_HasOutParms),
			TEXT("reference input plus return should set FUNC_HasOutParms")));

		FIntProperty* OutCountParam = CastField<FIntProperty>(FindParameterForTest(FillOutRefs, TEXT("Count")));
		FStrProperty* OutLabelParam = CastField<FStrProperty>(FindParameterForTest(FillOutRefs, TEXT("Label")));
		FStructProperty* OutLocationParam = CastField<FStructProperty>(FindParameterForTest(FillOutRefs, TEXT("Location")));
		ASSERT_THAT(IsNotNull(OutCountParam, TEXT("int &out should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(OutLabelParam, TEXT("FString &out should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(OutLocationParam, TEXT("FVector &out should reflect as FStructProperty")));
		if (OutCountParam == nullptr || OutLabelParam == nullptr || OutLocationParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(OutCountParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("int &out should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(OutCountParam->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReturnParm),
			TEXT("int &out should not carry const or return flags")));
		ASSERT_THAT(IsTrue(OutLabelParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("FString &out should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(OutLabelParam->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReturnParm),
			TEXT("FString &out should not carry const or return flags")));
		ASSERT_THAT(IsTrue(OutLocationParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("FVector &out should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(OutLocationParam->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReturnParm),
			TEXT("FVector &out should not carry const or return flags")));
		ASSERT_THAT(IsTrue(FillOutRefs->HasAnyFunctionFlags(FUNC_HasOutParms),
			TEXT("out-parameter UFUNCTION should set FUNC_HasOutParms")));

		FIntProperty* InoutCountParam = CastField<FIntProperty>(FindParameterForTest(MutateInoutRefs, TEXT("Count")));
		FStrProperty* InoutLabelParam = CastField<FStrProperty>(FindParameterForTest(MutateInoutRefs, TEXT("Label")));
		FStructProperty* InoutLocationParam = CastField<FStructProperty>(FindParameterForTest(MutateInoutRefs, TEXT("Location")));
		FIntProperty* InoutReturnParam = CastField<FIntProperty>(FindParameterForTest(MutateInoutRefs, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(InoutCountParam, TEXT("int &inout should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(InoutLabelParam, TEXT("FString &inout should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(InoutLocationParam, TEXT("FVector &inout should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(InoutReturnParam, TEXT("MutateInoutRefs return should reflect as FIntProperty")));
		if (InoutCountParam == nullptr || InoutLabelParam == nullptr || InoutLocationParam == nullptr || InoutReturnParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(InoutCountParam->HasAllPropertyFlags(CPF_OutParm | CPF_ReferenceParm),
			TEXT("int &inout should carry out/reference flags")));
		ASSERT_THAT(IsFalse(InoutCountParam->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReturnParm),
			TEXT("int &inout should not carry const or return flags")));
		ASSERT_THAT(IsTrue(InoutLabelParam->HasAllPropertyFlags(CPF_OutParm | CPF_ReferenceParm),
			TEXT("FString &inout should carry out/reference flags")));
		ASSERT_THAT(IsTrue(InoutLocationParam->HasAllPropertyFlags(CPF_OutParm | CPF_ReferenceParm),
			TEXT("FVector &inout should carry out/reference flags")));
		ASSERT_THAT(IsTrue(InoutReturnParam->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("MutateInoutRefs return should carry CPF_ReturnParm")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("reference-direction actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker ReadInvoker(*TestRunner, Actor, TEXT("ReadConstRefs"));
		ASSERT_THAT(IsTrue(ReadInvoker.IsValid(), TEXT("ReadConstRefs should be invokable through reflection")));
		if (!ReadInvoker.IsValid())
		{
			return;
		}
		ReadInvoker.AddParam<int32>(30);
		ReadInvoker.AddParam<FString>(FString(TEXT("Input")));
		ReadInvoker.AddParam<FVector>(FVector(7.0, 8.0, 9.0));
		ASSERT_THAT(AreEqual(42, ReadInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("const reference inputs should execute and return computed result")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastReadScore"), 42,
			TEXT("const reference call should update int state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastLabel"), FString(TEXT("Input")),
			TEXT("const FString &in should round-trip into script state"))));
		FVector LastVector = FVector::ZeroVector;
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("LastVector"), LastVector),
			TEXT("LastVector should be readable after const reference call")));
		ASSERT_THAT(IsTrue(LastVector.Equals(FVector(7.0, 8.0, 9.0), 0.001),
			TEXT("const FVector &in should round-trip into script state")));

		FFunctionInvoker OutInvoker(*TestRunner, Actor, TEXT("FillOutRefs"));
		ASSERT_THAT(IsTrue(OutInvoker.IsValid(), TEXT("FillOutRefs should be invokable through reflection")));
		if (!OutInvoker.IsValid())
		{
			return;
		}
		FProperty* OutCountProperty = nullptr;
		void* OutCountSlot = nullptr;
		FProperty* OutLabelProperty = nullptr;
		void* OutLabelSlot = nullptr;
		FProperty* OutLocationProperty = nullptr;
		void* OutLocationSlot = nullptr;
		ASSERT_THAT(IsTrue(OutInvoker.AddParamSlot(OutCountProperty, OutCountSlot), TEXT("FillOutRefs should expose int &out slot")));
		ASSERT_THAT(IsTrue(OutInvoker.AddParamSlot(OutLabelProperty, OutLabelSlot), TEXT("FillOutRefs should expose FString &out slot")));
		ASSERT_THAT(IsTrue(OutInvoker.AddParamSlot(OutLocationProperty, OutLocationSlot), TEXT("FillOutRefs should expose FVector &out slot")));
		FIntProperty* OutCountSlotProperty = CastField<FIntProperty>(OutCountProperty);
		FStrProperty* OutLabelSlotProperty = CastField<FStrProperty>(OutLabelProperty);
		FStructProperty* OutLocationSlotProperty = CastField<FStructProperty>(OutLocationProperty);
		ASSERT_THAT(IsNotNull(OutCountSlotProperty, TEXT("FillOutRefs int &out slot should be FIntProperty")));
		ASSERT_THAT(IsNotNull(OutLabelSlotProperty, TEXT("FillOutRefs FString &out slot should be FStrProperty")));
		ASSERT_THAT(IsNotNull(OutLocationSlotProperty, TEXT("FillOutRefs FVector &out slot should be FStructProperty")));
		if (OutCountSlot == nullptr || OutLabelSlot == nullptr || OutLocationSlot == nullptr
			|| OutCountSlotProperty == nullptr || OutLabelSlotProperty == nullptr || OutLocationSlotProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(OutInvoker.Call(), TEXT("FillOutRefs should write all reflected out parameters")));
		ASSERT_THAT(AreEqual(42, OutCountSlotProperty->GetPropertyValue(OutCountSlot),
			TEXT("int &out should write the caller buffer")));
		ASSERT_THAT(AreEqual(FString(TEXT("OutLabel")), OutLabelSlotProperty->GetPropertyValue(OutLabelSlot),
			TEXT("FString &out should write the caller buffer")));
		const FVector& OutLocationValue = *static_cast<const FVector*>(OutLocationSlot);
		ASSERT_THAT(IsTrue(OutLocationValue.Equals(FVector(4.0, 5.0, 6.0), 0.001),
			TEXT("FVector &out should write the caller buffer")));

		FFunctionInvoker InoutInvoker(*TestRunner, Actor, TEXT("MutateInoutRefs"));
		ASSERT_THAT(IsTrue(InoutInvoker.IsValid(), TEXT("MutateInoutRefs should be invokable through reflection")));
		if (!InoutInvoker.IsValid())
		{
			return;
		}
		FProperty* InoutCountProperty = nullptr;
		void* InoutCountSlot = nullptr;
		FProperty* InoutLabelProperty = nullptr;
		void* InoutLabelSlot = nullptr;
		FProperty* InoutLocationProperty = nullptr;
		void* InoutLocationSlot = nullptr;
		ASSERT_THAT(IsTrue(InoutInvoker.AddParamSlot(InoutCountProperty, InoutCountSlot), TEXT("MutateInoutRefs should expose int &inout slot")));
		ASSERT_THAT(IsTrue(InoutInvoker.AddParamSlot(InoutLabelProperty, InoutLabelSlot), TEXT("MutateInoutRefs should expose FString &inout slot")));
		ASSERT_THAT(IsTrue(InoutInvoker.AddParamSlot(InoutLocationProperty, InoutLocationSlot), TEXT("MutateInoutRefs should expose FVector &inout slot")));
		FIntProperty* InoutCountSlotProperty = CastField<FIntProperty>(InoutCountProperty);
		FStrProperty* InoutLabelSlotProperty = CastField<FStrProperty>(InoutLabelProperty);
		FStructProperty* InoutLocationSlotProperty = CastField<FStructProperty>(InoutLocationProperty);
		ASSERT_THAT(IsNotNull(InoutCountSlotProperty, TEXT("MutateInoutRefs int &inout slot should be FIntProperty")));
		ASSERT_THAT(IsNotNull(InoutLabelSlotProperty, TEXT("MutateInoutRefs FString &inout slot should be FStrProperty")));
		ASSERT_THAT(IsNotNull(InoutLocationSlotProperty, TEXT("MutateInoutRefs FVector &inout slot should be FStructProperty")));
		if (InoutCountSlot == nullptr || InoutLabelSlot == nullptr || InoutLocationSlot == nullptr
			|| InoutCountSlotProperty == nullptr || InoutLabelSlotProperty == nullptr || InoutLocationSlotProperty == nullptr)
		{
			return;
		}
		InoutCountSlotProperty->SetPropertyValue(InoutCountSlot, 10);
		InoutLabelSlotProperty->SetPropertyValue(InoutLabelSlot, FString(TEXT("InLabel")));
		*static_cast<FVector*>(InoutLocationSlot) = FVector(1.0, 2.0, 3.0);
		ASSERT_THAT(AreEqual(36, InoutInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("inout reference UFUNCTION should return from mutated state")));
		ASSERT_THAT(AreEqual(15, InoutCountSlotProperty->GetPropertyValue(InoutCountSlot),
			TEXT("int &inout should mutate the caller buffer")));
		ASSERT_THAT(AreEqual(FString(TEXT("InLabel|Mutated")), InoutLabelSlotProperty->GetPropertyValue(InoutLabelSlot),
			TEXT("FString &inout should mutate the caller buffer")));
		const FVector& InoutLocationValue = *static_cast<const FVector*>(InoutLocationSlot);
		ASSERT_THAT(IsTrue(InoutLocationValue.Equals(FVector(3.0, 5.0, 7.0), 0.001),
			TEXT("FVector &inout should mutate the caller buffer")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bInoutSawOriginal"), true,
			TEXT("inout UFUNCTION should read caller-provided values before mutation"))));
	}

	TEST_METHOD(BlueprintPureOutOnlyAndInoutRuntimeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_PureOutOnlyInout"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionPureOutActor : AActor
			{
				UPROPERTY()
				int StoredValue = 17;

				UPROPERTY()
				FString StoredLabel = "Base";

				UFUNCTION(BlueprintPure, Category="Coverage|PureOut", meta=(DisplayName="Fill Pure Out"))
				void FillPureOut(int Seed, int&out OutValue, FString&out OutLabel) const
				{
					OutValue = StoredValue + Seed;
					OutLabel = StoredLabel + ":" + Seed;
				}

				UFUNCTION(BlueprintPure, Category="Coverage|PureOut")
				void MutatePureInout(int&inout Value, FString&inout Label) const
				{
					Value += StoredValue;
					Label = Label + ":" + StoredLabel;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|PureOut")
				int DispatchPureOutMatrix()
				{
					int OutValue = 0;
					FString OutLabel;
					FillPureOut(5, OutValue, OutLabel);

					int InOutValue = 3;
					FString InOutLabel = "Input";
					MutatePureInout(InOutValue, InOutLabel);
					StoredLabel = OutLabel + "|" + InOutLabel;
					return OutValue + InOutValue + StoredLabel.Len();
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionPureOutOnlyInout.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionPureOutActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("BlueprintPure out-only actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* FillPureOut = FindFunctionForTest(ScriptClass, TEXT("FillPureOut"));
		UFunction* MutatePureInout = FindFunctionForTest(ScriptClass, TEXT("MutatePureInout"));
		UFunction* DispatchPureOutMatrix = FindFunctionForTest(ScriptClass, TEXT("DispatchPureOutMatrix"));
		ASSERT_THAT(IsNotNull(FillPureOut, TEXT("FillPureOut should be generated")));
		ASSERT_THAT(IsNotNull(MutatePureInout, TEXT("MutatePureInout should be generated")));
		ASSERT_THAT(IsNotNull(DispatchPureOutMatrix, TEXT("DispatchPureOutMatrix should be generated")));
		if (FillPureOut == nullptr || MutatePureInout == nullptr || DispatchPureOutMatrix == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(HasAllFunctionFlags(FillPureOut, FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_Const | FUNC_HasOutParms),
			TEXT("BlueprintPure void function with out params should carry callable/pure/const/out flags")));
		ASSERT_THAT(IsTrue(HasAllFunctionFlags(MutatePureInout, FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_Const | FUNC_HasOutParms),
			TEXT("BlueprintPure void function with inout params should carry callable/pure/const/out flags")));
		ASSERT_THAT(AreEqual(FString(TEXT("Fill Pure Out")), FillPureOut->GetMetaData(TEXT("DisplayName")),
			TEXT("pure out-only DisplayName metadata should round-trip")));
		ASSERT_THAT(IsNull(FillPureOut->GetReturnProperty(),
			TEXT("pure out-only void function should not expose a return property")));
		ASSERT_THAT(IsNull(MutatePureInout->GetReturnProperty(),
			TEXT("pure inout void function should not expose a return property")));

		FIntProperty* SeedParam = CastField<FIntProperty>(FindParameterForTest(FillPureOut, TEXT("Seed")));
		FIntProperty* OutValueParam = CastField<FIntProperty>(FindParameterForTest(FillPureOut, TEXT("OutValue")));
		FStrProperty* OutLabelParam = CastField<FStrProperty>(FindParameterForTest(FillPureOut, TEXT("OutLabel")));
		FIntProperty* InoutValueParam = CastField<FIntProperty>(FindParameterForTest(MutatePureInout, TEXT("Value")));
		FStrProperty* InoutLabelParam = CastField<FStrProperty>(FindParameterForTest(MutatePureInout, TEXT("Label")));
		ASSERT_THAT(IsNotNull(SeedParam, TEXT("pure out-only seed parameter should reflect")));
		ASSERT_THAT(IsNotNull(OutValueParam, TEXT("pure out-only int output should reflect")));
		ASSERT_THAT(IsNotNull(OutLabelParam, TEXT("pure out-only string output should reflect")));
		ASSERT_THAT(IsNotNull(InoutValueParam, TEXT("pure inout int parameter should reflect")));
		ASSERT_THAT(IsNotNull(InoutLabelParam, TEXT("pure inout string parameter should reflect")));
		if (SeedParam == nullptr || OutValueParam == nullptr || OutLabelParam == nullptr
			|| InoutValueParam == nullptr || InoutLabelParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(SeedParam->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm),
			TEXT("pure out-only seed should remain a regular input parameter")));
		ASSERT_THAT(IsTrue(OutValueParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("pure out-only int output should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(OutValueParam->HasAnyPropertyFlags(CPF_ReferenceParm | CPF_ConstParm),
			TEXT("pure out-only int output should not carry reference or const flags")));
		ASSERT_THAT(IsTrue(OutLabelParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("pure out-only string output should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(OutLabelParam->HasAnyPropertyFlags(CPF_ReferenceParm | CPF_ConstParm),
			TEXT("pure out-only string output should not carry reference or const flags")));
		ASSERT_THAT(IsTrue(InoutValueParam->HasAllPropertyFlags(CPF_OutParm | CPF_ReferenceParm),
			TEXT("pure int &inout parameter should carry out/reference flags")));
		ASSERT_THAT(IsTrue(InoutLabelParam->HasAllPropertyFlags(CPF_OutParm | CPF_ReferenceParm),
			TEXT("pure FString &inout parameter should carry out/reference flags")));
		ASSERT_THAT(IsFalse(InoutValueParam->HasAnyPropertyFlags(CPF_ConstParm),
			TEXT("pure int &inout parameter should not carry const flags")));
		ASSERT_THAT(IsFalse(InoutLabelParam->HasAnyPropertyFlags(CPF_ConstParm),
			TEXT("pure FString &inout parameter should not carry const flags")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("BlueprintPure out-only actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker FillInvoker(*TestRunner, Actor, TEXT("FillPureOut"));
		ASSERT_THAT(IsTrue(FillInvoker.IsValid(), TEXT("FillPureOut should be reflectively invokable")));
		if (!FillInvoker.IsValid())
		{
			return;
		}
		FillInvoker.AddParam<int32>(8);
		FProperty* OutValueProperty = nullptr;
		void* OutValueSlot = nullptr;
		FProperty* OutLabelProperty = nullptr;
		void* OutLabelSlot = nullptr;
		ASSERT_THAT(IsTrue(FillInvoker.AddParamSlot(OutValueProperty, OutValueSlot), TEXT("FillPureOut should expose int out slot")));
		ASSERT_THAT(IsTrue(FillInvoker.AddParamSlot(OutLabelProperty, OutLabelSlot), TEXT("FillPureOut should expose FString out slot")));
		FIntProperty* RuntimeOutValue = CastField<FIntProperty>(OutValueProperty);
		FStrProperty* RuntimeOutLabel = CastField<FStrProperty>(OutLabelProperty);
		ASSERT_THAT(IsNotNull(RuntimeOutValue, TEXT("FillPureOut int out slot should be FIntProperty")));
		ASSERT_THAT(IsNotNull(RuntimeOutLabel, TEXT("FillPureOut FString out slot should be FStrProperty")));
		if (OutValueSlot == nullptr || OutLabelSlot == nullptr || RuntimeOutValue == nullptr || RuntimeOutLabel == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(FillInvoker.Call(), TEXT("FillPureOut should execute through reflection")));
		ASSERT_THAT(AreEqual(25, RuntimeOutValue->GetPropertyValue(OutValueSlot),
			TEXT("FillPureOut should write int output")));
		ASSERT_THAT(AreEqual(FString(TEXT("Base:8")), RuntimeOutLabel->GetPropertyValue(OutLabelSlot),
			TEXT("FillPureOut should write FString output")));

		FFunctionInvoker InoutInvoker(*TestRunner, Actor, TEXT("MutatePureInout"));
		ASSERT_THAT(IsTrue(InoutInvoker.IsValid(), TEXT("MutatePureInout should be reflectively invokable")));
		if (!InoutInvoker.IsValid())
		{
			return;
		}
		FProperty* InoutValueProperty = nullptr;
		void* InoutValueSlot = nullptr;
		FProperty* InoutLabelProperty = nullptr;
		void* InoutLabelSlot = nullptr;
		ASSERT_THAT(IsTrue(InoutInvoker.AddParamSlot(InoutValueProperty, InoutValueSlot),
			TEXT("MutatePureInout should expose int inout slot")));
		ASSERT_THAT(IsTrue(InoutInvoker.AddParamSlot(InoutLabelProperty, InoutLabelSlot),
			TEXT("MutatePureInout should expose FString inout slot")));
		FIntProperty* RuntimeInoutValue = CastField<FIntProperty>(InoutValueProperty);
		FStrProperty* RuntimeInoutLabel = CastField<FStrProperty>(InoutLabelProperty);
		ASSERT_THAT(IsNotNull(RuntimeInoutValue, TEXT("MutatePureInout int slot should be FIntProperty")));
		ASSERT_THAT(IsNotNull(RuntimeInoutLabel, TEXT("MutatePureInout FString slot should be FStrProperty")));
		if (InoutValueSlot == nullptr || InoutLabelSlot == nullptr || RuntimeInoutValue == nullptr || RuntimeInoutLabel == nullptr)
		{
			return;
		}
		RuntimeInoutValue->SetPropertyValue(InoutValueSlot, 4);
		RuntimeInoutLabel->SetPropertyValue(InoutLabelSlot, FString(TEXT("Seed")));
		ASSERT_THAT(IsTrue(InoutInvoker.Call(), TEXT("MutatePureInout should execute through reflection")));
		ASSERT_THAT(AreEqual(21, RuntimeInoutValue->GetPropertyValue(InoutValueSlot),
			TEXT("MutatePureInout should mutate int caller buffer")));
		ASSERT_THAT(AreEqual(FString(TEXT("Seed:Base")), RuntimeInoutLabel->GetPropertyValue(InoutLabelSlot),
			TEXT("MutatePureInout should mutate FString caller buffer")));

		FFunctionInvoker DispatchInvoker(*TestRunner, Actor, TEXT("DispatchPureOutMatrix"));
		ASSERT_THAT(IsTrue(DispatchInvoker.IsValid(), TEXT("DispatchPureOutMatrix should be reflectively invokable")));
		if (!DispatchInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(59, DispatchInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("pure out/inout dispatcher should combine outputs and mutated values")));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StoredLabel"),
			FString(TEXT("Base:5|Input:Base")), TEXT("pure out/inout dispatcher should store composed labels"))));
	}

	TEST_METHOD(ReturnAndOutParameterPermutationMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_ReturnOutPermutations"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FUFunctionReturnOutPayload
			{
				UPROPERTY()
				int Count = 0;

				UPROPERTY()
				FString Label;
			}

			UCLASS()
			class ACoverageUFunctionReturnOutActor : AActor
			{
				UPROPERTY()
				int LastScore = 0;

				UPROPERTY()
				FString LastLabel;

				UFUNCTION(BlueprintCallable, Category="Coverage|ReturnOut")
				int ReturnAndWritePrimitiveOuts(int BaseValue, int&out OutInt, bool&out bOutBool, FString&out OutLabel)
				{
					OutInt = BaseValue + 10;
					bOutBool = BaseValue > 0;
					OutLabel = "PrimitiveOut";
					LastScore = BaseValue + OutInt + (bOutBool ? 1 : 0) + OutLabel.Len();
					LastLabel = OutLabel;
					return LastScore;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|ReturnOut")
				FUFunctionReturnOutPayload ReturnStructAndWriteMixedOuts(int BaseValue, FUFunctionReturnOutPayload&out OutPayload, FVector&out OutLocation, int&inout InOutScore)
				{
					OutPayload.Count = BaseValue + 20;
					OutPayload.Label = "OutPayload";
					OutLocation = FVector(BaseValue, BaseValue + 1, BaseValue + 2);
					InOutScore += OutPayload.Count + int(OutLocation.X);

					FUFunctionReturnOutPayload ReturnPayload;
					ReturnPayload.Count = InOutScore;
					ReturnPayload.Label = "ReturnPayload";
					LastScore = InOutScore;
					LastLabel = ReturnPayload.Label;
					return ReturnPayload;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionReturnOutPermutations.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionReturnOutActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("return/out permutation actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* ReturnAndWritePrimitiveOuts = FindFunctionForTest(ScriptClass, TEXT("ReturnAndWritePrimitiveOuts"));
		UFunction* ReturnStructAndWriteMixedOuts = FindFunctionForTest(ScriptClass, TEXT("ReturnStructAndWriteMixedOuts"));
		ASSERT_THAT(IsNotNull(ReturnAndWritePrimitiveOuts, TEXT("primitive return/out UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnStructAndWriteMixedOuts, TEXT("struct return/mixed out UFUNCTION should be generated")));
		if (ReturnAndWritePrimitiveOuts == nullptr || ReturnStructAndWriteMixedOuts == nullptr)
		{
			return;
		}

		FIntProperty* PrimitiveBaseParam = CastField<FIntProperty>(FindParameterForTest(ReturnAndWritePrimitiveOuts, TEXT("BaseValue")));
		FIntProperty* PrimitiveOutIntParam = CastField<FIntProperty>(FindParameterForTest(ReturnAndWritePrimitiveOuts, TEXT("OutInt")));
		FBoolProperty* PrimitiveOutBoolParam = CastField<FBoolProperty>(FindParameterForTest(ReturnAndWritePrimitiveOuts, TEXT("bOutBool")));
		FStrProperty* PrimitiveOutLabelParam = CastField<FStrProperty>(FindParameterForTest(ReturnAndWritePrimitiveOuts, TEXT("OutLabel")));
		FIntProperty* PrimitiveReturnParam = CastField<FIntProperty>(FindParameterForTest(ReturnAndWritePrimitiveOuts, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(PrimitiveBaseParam, TEXT("primitive permutation input should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(PrimitiveOutIntParam, TEXT("primitive permutation int out should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(PrimitiveOutBoolParam, TEXT("primitive permutation bool out should reflect as FBoolProperty")));
		ASSERT_THAT(IsNotNull(PrimitiveOutLabelParam, TEXT("primitive permutation FString out should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(PrimitiveReturnParam, TEXT("primitive permutation return should reflect as FIntProperty")));
		if (PrimitiveBaseParam == nullptr || PrimitiveOutIntParam == nullptr || PrimitiveOutBoolParam == nullptr
			|| PrimitiveOutLabelParam == nullptr || PrimitiveReturnParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(PrimitiveBaseParam->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm),
			TEXT("primitive permutation input should remain plain input")));
		ASSERT_THAT(IsTrue(PrimitiveOutIntParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("primitive int out should carry CPF_OutParm")));
		ASSERT_THAT(IsTrue(PrimitiveOutBoolParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("primitive bool out should carry CPF_OutParm")));
		ASSERT_THAT(IsTrue(PrimitiveOutLabelParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("primitive FString out should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(PrimitiveOutLabelParam->HasAnyPropertyFlags(CPF_ConstParm),
			TEXT("FString &out should not carry CPF_ConstParm")));
		ASSERT_THAT(IsTrue(PrimitiveReturnParam->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("primitive return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(ReturnAndWritePrimitiveOuts->HasAnyFunctionFlags(FUNC_HasOutParms),
			TEXT("primitive return/out function should carry FUNC_HasOutParms")));

		FIntProperty* StructBaseParam = CastField<FIntProperty>(FindParameterForTest(ReturnStructAndWriteMixedOuts, TEXT("BaseValue")));
		FStructProperty* StructOutPayloadParam = CastField<FStructProperty>(FindParameterForTest(ReturnStructAndWriteMixedOuts, TEXT("OutPayload")));
		FStructProperty* StructOutLocationParam = CastField<FStructProperty>(FindParameterForTest(ReturnStructAndWriteMixedOuts, TEXT("OutLocation")));
		FIntProperty* StructInOutScoreParam = CastField<FIntProperty>(FindParameterForTest(ReturnStructAndWriteMixedOuts, TEXT("InOutScore")));
		FStructProperty* StructReturnParam = CastField<FStructProperty>(FindParameterForTest(ReturnStructAndWriteMixedOuts, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(StructBaseParam, TEXT("struct permutation input should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(StructOutPayloadParam, TEXT("AS USTRUCT out should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(StructOutLocationParam, TEXT("FVector out should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(StructInOutScoreParam, TEXT("int inout should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(StructReturnParam, TEXT("AS USTRUCT return should reflect as FStructProperty")));
		if (StructBaseParam == nullptr || StructOutPayloadParam == nullptr || StructOutLocationParam == nullptr
			|| StructInOutScoreParam == nullptr || StructReturnParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(StructOutPayloadParam->Struct, StructReturnParam->Struct,
			TEXT("AS USTRUCT out and return should share generated struct type")));
		ASSERT_THAT(AreEqual(TBaseStructure<FVector>::Get(), StructOutLocationParam->Struct,
			TEXT("FVector out parameter should preserve native FVector struct type")));
		ASSERT_THAT(IsTrue(StructOutPayloadParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("AS USTRUCT &out should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(StructOutPayloadParam->HasAnyPropertyFlags(CPF_ConstParm),
			TEXT("AS USTRUCT &out should not carry CPF_ConstParm")));
		ASSERT_THAT(IsTrue(StructOutLocationParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("FVector &out should carry CPF_OutParm")));
		ASSERT_THAT(IsTrue(StructInOutScoreParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("int &inout should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(StructInOutScoreParam->HasAnyPropertyFlags(CPF_ConstParm),
			TEXT("int &inout should not carry CPF_ConstParm")));
		ASSERT_THAT(IsTrue(StructReturnParam->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("AS USTRUCT return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(ReturnStructAndWriteMixedOuts->HasAnyFunctionFlags(FUNC_HasOutParms),
			TEXT("struct return/out function should carry FUNC_HasOutParms")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("return/out permutation actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker PrimitiveInvoker(*TestRunner, Actor, TEXT("ReturnAndWritePrimitiveOuts"));
		ASSERT_THAT(IsTrue(PrimitiveInvoker.IsValid(), TEXT("ReturnAndWritePrimitiveOuts should be invokable")));
		if (!PrimitiveInvoker.IsValid())
		{
			return;
		}
		PrimitiveInvoker.AddParam<int32>(21);
		FProperty* OutIntSlotProperty = nullptr;
		void* OutIntSlot = nullptr;
		FProperty* OutBoolSlotProperty = nullptr;
		void* OutBoolSlot = nullptr;
		FProperty* OutLabelSlotProperty = nullptr;
		void* OutLabelSlot = nullptr;
		ASSERT_THAT(IsTrue(PrimitiveInvoker.AddParamSlot(OutIntSlotProperty, OutIntSlot),
			TEXT("ReturnAndWritePrimitiveOuts should expose int out slot")));
		ASSERT_THAT(IsTrue(PrimitiveInvoker.AddParamSlot(OutBoolSlotProperty, OutBoolSlot),
			TEXT("ReturnAndWritePrimitiveOuts should expose bool out slot")));
		ASSERT_THAT(IsTrue(PrimitiveInvoker.AddParamSlot(OutLabelSlotProperty, OutLabelSlot),
			TEXT("ReturnAndWritePrimitiveOuts should expose FString out slot")));
		FIntProperty* RuntimeOutIntParam = CastField<FIntProperty>(OutIntSlotProperty);
		FBoolProperty* RuntimeOutBoolParam = CastField<FBoolProperty>(OutBoolSlotProperty);
		FStrProperty* RuntimeOutLabelParam = CastField<FStrProperty>(OutLabelSlotProperty);
		ASSERT_THAT(IsNotNull(RuntimeOutIntParam, TEXT("runtime primitive int out slot should be FIntProperty")));
		ASSERT_THAT(IsNotNull(RuntimeOutBoolParam, TEXT("runtime primitive bool out slot should be FBoolProperty")));
		ASSERT_THAT(IsNotNull(RuntimeOutLabelParam, TEXT("runtime primitive FString out slot should be FStrProperty")));
		if (OutIntSlot == nullptr || OutBoolSlot == nullptr || OutLabelSlot == nullptr
			|| RuntimeOutIntParam == nullptr || RuntimeOutBoolParam == nullptr || RuntimeOutLabelParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(65, PrimitiveInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("primitive return/out permutation should return computed score")));
		ASSERT_THAT(AreEqual(31, RuntimeOutIntParam->GetPropertyValue(OutIntSlot),
			TEXT("primitive int out should be written")));
		ASSERT_THAT(IsTrue(RuntimeOutBoolParam->GetPropertyValue(OutBoolSlot),
			TEXT("primitive bool out should be written")));
		ASSERT_THAT(AreEqual(FString(TEXT("PrimitiveOut")), RuntimeOutLabelParam->GetPropertyValue(OutLabelSlot),
			TEXT("primitive FString out should be written")));

		FFunctionInvoker StructInvoker(*TestRunner, Actor, TEXT("ReturnStructAndWriteMixedOuts"));
		ASSERT_THAT(IsTrue(StructInvoker.IsValid(), TEXT("ReturnStructAndWriteMixedOuts should be invokable")));
		if (!StructInvoker.IsValid())
		{
			return;
		}
		StructInvoker.AddParam<int32>(12);
		FProperty* OutPayloadSlotProperty = nullptr;
		void* OutPayloadSlot = nullptr;
		FProperty* OutLocationSlotProperty = nullptr;
		void* OutLocationSlot = nullptr;
		FProperty* InOutScoreSlotProperty = nullptr;
		void* InOutScoreSlot = nullptr;
		ASSERT_THAT(IsTrue(StructInvoker.AddParamSlot(OutPayloadSlotProperty, OutPayloadSlot),
			TEXT("ReturnStructAndWriteMixedOuts should expose AS USTRUCT out slot")));
		ASSERT_THAT(IsTrue(StructInvoker.AddParamSlot(OutLocationSlotProperty, OutLocationSlot),
			TEXT("ReturnStructAndWriteMixedOuts should expose FVector out slot")));
		ASSERT_THAT(IsTrue(StructInvoker.AddParamSlot(InOutScoreSlotProperty, InOutScoreSlot),
			TEXT("ReturnStructAndWriteMixedOuts should expose int inout slot")));
		FStructProperty* RuntimeOutPayloadParam = CastField<FStructProperty>(OutPayloadSlotProperty);
		FStructProperty* RuntimeOutLocationParam = CastField<FStructProperty>(OutLocationSlotProperty);
		FIntProperty* RuntimeInOutScoreParam = CastField<FIntProperty>(InOutScoreSlotProperty);
		ASSERT_THAT(IsNotNull(RuntimeOutPayloadParam, TEXT("runtime AS USTRUCT out slot should be FStructProperty")));
		ASSERT_THAT(IsNotNull(RuntimeOutLocationParam, TEXT("runtime FVector out slot should be FStructProperty")));
		ASSERT_THAT(IsNotNull(RuntimeInOutScoreParam, TEXT("runtime int inout slot should be FIntProperty")));
		if (OutPayloadSlot == nullptr || OutLocationSlot == nullptr || InOutScoreSlot == nullptr
			|| RuntimeOutPayloadParam == nullptr || RuntimeOutLocationParam == nullptr || RuntimeInOutScoreParam == nullptr)
		{
			return;
		}
		RuntimeInOutScoreParam->SetPropertyValue(InOutScoreSlot, -2);
		ASSERT_THAT(IsTrue(StructInvoker.Call(), TEXT("ReturnStructAndWriteMixedOuts should execute")));
		void* ReturnPayloadSlot = StructReturnParam->ContainerPtrToValuePtr<void>(StructInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnPayloadSlot, TEXT("struct return payload slot should be readable")));
		if (ReturnPayloadSlot == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VerifyPayloadStructValue(*TestRunner, *RuntimeOutPayloadParam, OutPayloadSlot, 32, FString(TEXT("OutPayload")),
			TEXT("mixed out payload"))));
		const FVector& OutLocationValue = *static_cast<const FVector*>(OutLocationSlot);
		ASSERT_THAT(IsTrue(OutLocationValue.Equals(FVector(12.0, 13.0, 14.0), 0.001),
			TEXT("mixed FVector out should be written")));
		ASSERT_THAT(AreEqual(42, RuntimeInOutScoreParam->GetPropertyValue(InOutScoreSlot),
			TEXT("mixed int inout should be updated")));
		ASSERT_THAT(IsTrue(VerifyPayloadStructValue(*TestRunner, *StructReturnParam, ReturnPayloadSlot, 42, FString(TEXT("ReturnPayload")),
			TEXT("mixed return payload"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastScore"), 42,
			TEXT("return/out permutation should update int state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastLabel"), FString(TEXT("ReturnPayload")),
			TEXT("return/out permutation should update string state"))));
	}

	TEST_METHOD(ContainerParameterAndReturnReflectionMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_ContainerParameterReturn"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionContainerActor : AActor
			{
				UPROPERTY()
				int LastArrayCount = 0;

				UPROPERTY()
				int LastMapScore = 0;

				UPROPERTY()
				bool bSetInoutSawOriginal = false;

				UFUNCTION(BlueprintCallable, Category="Coverage|Containers")
				int CountArray(const TArray<int>&in Values)
				{
					LastArrayCount = Values.Num();
					int Total = 0;
					for (int Index = 0; Index < Values.Num(); ++Index)
					{
						Total += Values[Index];
					}
					return Total;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Containers")
				void FillArray(TArray<int>&out Values)
				{
					Values.Add(7);
					Values.Add(11);
					Values.Add(13);
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Containers")
				int ScoreMap(TMap<FName, int> Scores)
				{
					int Alpha = 0;
					int Beta = 0;
					Scores.Find(n"Alpha", Alpha);
					Scores.Find(n"Beta", Beta);
					LastMapScore = Alpha + Beta;
					return LastMapScore + Scores.Num();
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Containers")
				void FillMap(TMap<FName, int>&out Scores)
				{
					Scores.Add(n"Gamma", 17);
					Scores.Add(n"Delta", 19);
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Containers")
				int MutateSet(TSet<int>&inout Values)
				{
					bSetInoutSawOriginal = Values.Contains(5);
					Values.Add(42);
					return Values.Num();
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Containers")
				TArray<int> ReturnArray()
				{
					TArray<int> Values;
					Values.Add(3);
					Values.Add(4);
					Values.Add(5);
					return Values;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Containers")
				TMap<FName, int> ReturnMap()
				{
					TMap<FName, int> Scores;
					Scores.Add(n"ReturnA", 23);
					Scores.Add(n"ReturnB", 29);
					return Scores;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Containers")
				TSet<int> ReturnSet()
				{
					TSet<int> Values;
					Values.Add(31);
					Values.Add(37);
					return Values;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionContainerParameterReturn.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("container UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* CountArray = FindFunctionForTest(ScriptClass, TEXT("CountArray"));
		UFunction* FillArray = FindFunctionForTest(ScriptClass, TEXT("FillArray"));
		UFunction* ScoreMap = FindFunctionForTest(ScriptClass, TEXT("ScoreMap"));
		UFunction* FillMap = FindFunctionForTest(ScriptClass, TEXT("FillMap"));
		UFunction* MutateSet = FindFunctionForTest(ScriptClass, TEXT("MutateSet"));
		UFunction* ReturnArray = FindFunctionForTest(ScriptClass, TEXT("ReturnArray"));
		UFunction* ReturnMap = FindFunctionForTest(ScriptClass, TEXT("ReturnMap"));
		UFunction* ReturnSet = FindFunctionForTest(ScriptClass, TEXT("ReturnSet"));
		ASSERT_THAT(IsNotNull(CountArray, TEXT("CountArray should be generated")));
		ASSERT_THAT(IsNotNull(FillArray, TEXT("FillArray should be generated")));
		ASSERT_THAT(IsNotNull(ScoreMap, TEXT("ScoreMap should be generated")));
		ASSERT_THAT(IsNotNull(FillMap, TEXT("FillMap should be generated")));
		ASSERT_THAT(IsNotNull(MutateSet, TEXT("MutateSet should be generated")));
		ASSERT_THAT(IsNotNull(ReturnArray, TEXT("ReturnArray should be generated")));
		ASSERT_THAT(IsNotNull(ReturnMap, TEXT("ReturnMap should be generated")));
		ASSERT_THAT(IsNotNull(ReturnSet, TEXT("ReturnSet should be generated")));
		if (CountArray == nullptr || FillArray == nullptr || ScoreMap == nullptr || FillMap == nullptr
			|| MutateSet == nullptr || ReturnArray == nullptr || ReturnMap == nullptr || ReturnSet == nullptr)
		{
			return;
		}

		FArrayProperty* CountArrayParam = CastField<FArrayProperty>(FindParameterForTest(CountArray, TEXT("Values")));
		FIntProperty* CountArrayReturn = CastField<FIntProperty>(FindParameterForTest(CountArray, TEXT("ReturnValue")));
		FArrayProperty* FillArrayParam = CastField<FArrayProperty>(FindParameterForTest(FillArray, TEXT("Values")));
		FMapProperty* ScoreMapParam = CastField<FMapProperty>(FindParameterForTest(ScoreMap, TEXT("Scores")));
		FIntProperty* ScoreMapReturn = CastField<FIntProperty>(FindParameterForTest(ScoreMap, TEXT("ReturnValue")));
		FMapProperty* FillMapParam = CastField<FMapProperty>(FindParameterForTest(FillMap, TEXT("Scores")));
		FSetProperty* MutateSetParam = CastField<FSetProperty>(FindParameterForTest(MutateSet, TEXT("Values")));
		FIntProperty* MutateSetReturn = CastField<FIntProperty>(FindParameterForTest(MutateSet, TEXT("ReturnValue")));
		FArrayProperty* ReturnArrayParam = CastField<FArrayProperty>(FindParameterForTest(ReturnArray, TEXT("ReturnValue")));
		FMapProperty* ReturnMapParam = CastField<FMapProperty>(FindParameterForTest(ReturnMap, TEXT("ReturnValue")));
		FSetProperty* ReturnSetParam = CastField<FSetProperty>(FindParameterForTest(ReturnSet, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(CountArrayParam, TEXT("const TArray<int> &in should reflect as FArrayProperty")));
		ASSERT_THAT(IsNotNull(CountArrayReturn, TEXT("CountArray return should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(FillArrayParam, TEXT("TArray<int> &out should reflect as FArrayProperty")));
		ASSERT_THAT(IsNotNull(ScoreMapParam, TEXT("TMap<FName,int> parameter should reflect as FMapProperty")));
		ASSERT_THAT(IsNotNull(ScoreMapReturn, TEXT("ScoreMap return should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(FillMapParam, TEXT("TMap<FName,int> &out should reflect as FMapProperty")));
		ASSERT_THAT(IsNotNull(MutateSetParam, TEXT("TSet<int> &inout should reflect as FSetProperty")));
		ASSERT_THAT(IsNotNull(MutateSetReturn, TEXT("MutateSet return should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(ReturnArrayParam, TEXT("TArray<int> return should reflect as FArrayProperty")));
		ASSERT_THAT(IsNotNull(ReturnMapParam, TEXT("TMap<FName,int> return should reflect as FMapProperty")));
		ASSERT_THAT(IsNotNull(ReturnSetParam, TEXT("TSet<int> return should reflect as FSetProperty")));
		if (CountArrayParam == nullptr || CountArrayReturn == nullptr || FillArrayParam == nullptr || ScoreMapParam == nullptr
			|| ScoreMapReturn == nullptr || FillMapParam == nullptr || MutateSetParam == nullptr || MutateSetReturn == nullptr
			|| ReturnArrayParam == nullptr || ReturnMapParam == nullptr || ReturnSetParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(CountArrayParam->HasAllPropertyFlags(CPF_ConstParm | CPF_OutParm | CPF_ReferenceParm),
			TEXT("const TArray<int> &in should carry const/out/reference flags")));
		ASSERT_THAT(IsTrue(CastField<FIntProperty>(CountArrayParam->Inner) != nullptr,
			TEXT("TArray<int> parameter inner should be FIntProperty")));
		ASSERT_THAT(IsTrue(FillArrayParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("TArray<int> &out should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(FillArrayParam->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReturnParm),
			TEXT("TArray<int> &out should not carry const or return flags")));
		ASSERT_THAT(IsTrue(ScoreMapParam->KeyProp->IsA<FNameProperty>() && ScoreMapParam->ValueProp->IsA<FIntProperty>(),
			TEXT("TMap<FName,int> parameter should preserve key/value property types")));
		ASSERT_THAT(IsFalse(ScoreMapParam->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm),
			TEXT("TMap<FName,int> by-value parameter should remain a plain input")));
		ASSERT_THAT(IsTrue(FillMapParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("TMap<FName,int> &out should carry CPF_OutParm")));
		ASSERT_THAT(IsTrue(MutateSetParam->HasAllPropertyFlags(CPF_OutParm | CPF_ReferenceParm),
			TEXT("TSet<int> &inout should carry out/reference flags")));
		ASSERT_THAT(IsFalse(MutateSetParam->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReturnParm),
			TEXT("TSet<int> &inout should not carry const or return flags")));
		ASSERT_THAT(IsTrue(ReturnArrayParam->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("TArray<int> return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(ReturnMapParam->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("TMap<FName,int> return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(ReturnSetParam->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("TSet<int> return should carry CPF_ReturnParm")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("container UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FProperty* ParamProperty = nullptr;
		void* ParamSlot = nullptr;

		FFunctionInvoker CountArrayInvoker(*TestRunner, Actor, TEXT("CountArray"));
		ASSERT_THAT(IsTrue(CountArrayInvoker.IsValid(), TEXT("CountArray should be invokable")));
		if (!CountArrayInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(CountArrayInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountArray should expose TArray<int> const-ref parameter slot")));
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(ArrayProperty, TEXT("CountArray parameter slot should be FArrayProperty")));
		if (ParamSlot == nullptr || ArrayProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddIntArrayValue(*TestRunner, *ArrayProperty, ParamSlot, 10)));
		ASSERT_THAT(IsTrue(AddIntArrayValue(*TestRunner, *ArrayProperty, ParamSlot, 15)));
		ASSERT_THAT(IsTrue(AddIntArrayValue(*TestRunner, *ArrayProperty, ParamSlot, 17)));
		ASSERT_THAT(AreEqual(42, CountArrayInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("TArray<int> const-ref parameter should execute through reflection")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastArrayCount"), 3,
			TEXT("TArray<int> const-ref call should update script state"))));

		FFunctionInvoker FillArrayInvoker(*TestRunner, Actor, TEXT("FillArray"));
		ASSERT_THAT(IsTrue(FillArrayInvoker.IsValid(), TEXT("FillArray should be invokable")));
		if (!FillArrayInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(FillArrayInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillArray should expose TArray<int> out parameter slot")));
		ArrayProperty = CastField<FArrayProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(ArrayProperty, TEXT("FillArray parameter slot should be FArrayProperty")));
		if (ParamSlot == nullptr || ArrayProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(FillArrayInvoker.Call(), TEXT("FillArray should write reflected array out parameter")));
		FScriptArrayHelper FillArrayHelper(ArrayProperty, ParamSlot);
		ASSERT_THAT(AreEqual(3, FillArrayHelper.Num(), TEXT("TArray<int> &out should write three elements")));
		FIntProperty* ArrayInnerProperty = CastField<FIntProperty>(ArrayProperty->Inner);
		ASSERT_THAT(IsNotNull(ArrayInnerProperty, TEXT("TArray<int> &out inner should be FIntProperty")));
		if (ArrayInnerProperty == nullptr || FillArrayHelper.Num() < 3)
		{
			return;
		}
		ASSERT_THAT(AreEqual(31,
			ArrayInnerProperty->GetPropertyValue(FillArrayHelper.GetRawPtr(0))
			+ ArrayInnerProperty->GetPropertyValue(FillArrayHelper.GetRawPtr(1))
			+ ArrayInnerProperty->GetPropertyValue(FillArrayHelper.GetRawPtr(2)),
			TEXT("TArray<int> &out should preserve written element values")));

		FFunctionInvoker ScoreMapInvoker(*TestRunner, Actor, TEXT("ScoreMap"));
		ASSERT_THAT(IsTrue(ScoreMapInvoker.IsValid(), TEXT("ScoreMap should be invokable")));
		if (!ScoreMapInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ScoreMapInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("ScoreMap should expose TMap<FName,int> parameter slot")));
		FMapProperty* MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("ScoreMap parameter slot should be FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddNameIntMapValue(*TestRunner, *MapProperty, ParamSlot, FName(TEXT("Alpha")), 20)));
		ASSERT_THAT(IsTrue(AddNameIntMapValue(*TestRunner, *MapProperty, ParamSlot, FName(TEXT("Beta")), 20)));
		ASSERT_THAT(AreEqual(42, ScoreMapInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("TMap<FName,int> by-value parameter should execute through reflection")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastMapScore"), 40,
			TEXT("TMap<FName,int> parameter should update script state"))));

		FFunctionInvoker FillMapInvoker(*TestRunner, Actor, TEXT("FillMap"));
		ASSERT_THAT(IsTrue(FillMapInvoker.IsValid(), TEXT("FillMap should be invokable")));
		if (!FillMapInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(FillMapInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillMap should expose TMap<FName,int> out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("FillMap parameter slot should be FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(FillMapInvoker.Call(), TEXT("FillMap should write reflected map out parameter")));
		FScriptMapHelper FillMapHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, FillMapHelper.Num(), TEXT("TMap<FName,int> &out should write two entries")));
		int32 MapValue = 0;
		ASSERT_THAT(IsTrue(FindNameIntMapValue(*TestRunner, *MapProperty, ParamSlot, FName(TEXT("Delta")), MapValue)));
		ASSERT_THAT(AreEqual(19, MapValue, TEXT("TMap<FName,int> &out should preserve map value")));

		FFunctionInvoker MutateSetInvoker(*TestRunner, Actor, TEXT("MutateSet"));
		ASSERT_THAT(IsTrue(MutateSetInvoker.IsValid(), TEXT("MutateSet should be invokable")));
		if (!MutateSetInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MutateSetInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateSet should expose TSet<int> inout parameter slot")));
		FSetProperty* SetProperty = CastField<FSetProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(SetProperty, TEXT("MutateSet parameter slot should be FSetProperty")));
		if (ParamSlot == nullptr || SetProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddIntSetValue(*TestRunner, *SetProperty, ParamSlot, 5),
			TEXT("MutateSet caller buffer should accept an initial TSet<int> element")));
		ASSERT_THAT(AreEqual(2, MutateSetInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("TSet<int> &inout parameter should execute through reflection")));
		ASSERT_THAT(IsTrue(SetContainsIntValue(*TestRunner, *SetProperty, ParamSlot, 42),
			TEXT("TSet<int> &inout should contain script-added value")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSetInoutSawOriginal"), true,
			TEXT("TSet<int> &inout should read caller-provided values before mutation"))));

		FFunctionInvoker ReturnArrayInvoker(*TestRunner, Actor, TEXT("ReturnArray"));
		ASSERT_THAT(IsTrue(ReturnArrayInvoker.IsValid(), TEXT("ReturnArray should be invokable")));
		if (!ReturnArrayInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ReturnArrayInvoker.Call(), TEXT("ReturnArray should execute through reflection")));
		void* ReturnSlot = ReturnArrayParam->ContainerPtrToValuePtr<void>(ReturnArrayInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TArray<int> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptArrayHelper ReturnArrayHelper(ReturnArrayParam, ReturnSlot);
		ASSERT_THAT(AreEqual(3, ReturnArrayHelper.Num(), TEXT("TArray<int> return should contain three elements")));
		ArrayInnerProperty = CastField<FIntProperty>(ReturnArrayParam->Inner);
		ASSERT_THAT(IsNotNull(ArrayInnerProperty, TEXT("TArray<int> return inner should be FIntProperty")));
		if (ArrayInnerProperty == nullptr || ReturnArrayHelper.Num() < 3)
		{
			return;
		}
		ASSERT_THAT(AreEqual(12,
			ArrayInnerProperty->GetPropertyValue(ReturnArrayHelper.GetRawPtr(0))
			+ ArrayInnerProperty->GetPropertyValue(ReturnArrayHelper.GetRawPtr(1))
			+ ArrayInnerProperty->GetPropertyValue(ReturnArrayHelper.GetRawPtr(2)),
			TEXT("TArray<int> return should preserve element values")));

		FFunctionInvoker ReturnMapInvoker(*TestRunner, Actor, TEXT("ReturnMap"));
		ASSERT_THAT(IsTrue(ReturnMapInvoker.IsValid(), TEXT("ReturnMap should be invokable")));
		if (!ReturnMapInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ReturnMapInvoker.Call(), TEXT("ReturnMap should execute through reflection")));
		ReturnSlot = ReturnMapParam->ContainerPtrToValuePtr<void>(ReturnMapInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TMap<FName,int> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper ReturnMapHelper(ReturnMapParam, ReturnSlot);
		ASSERT_THAT(AreEqual(2, ReturnMapHelper.Num(), TEXT("TMap<FName,int> return should contain two entries")));
		MapValue = 0;
		ASSERT_THAT(IsTrue(FindNameIntMapValue(*TestRunner, *ReturnMapParam, ReturnSlot, FName(TEXT("ReturnB")), MapValue)));
		ASSERT_THAT(AreEqual(29, MapValue, TEXT("TMap<FName,int> return should preserve map values")));

		FFunctionInvoker ReturnSetInvoker(*TestRunner, Actor, TEXT("ReturnSet"));
		ASSERT_THAT(IsTrue(ReturnSetInvoker.IsValid(), TEXT("ReturnSet should be invokable")));
		if (!ReturnSetInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ReturnSetInvoker.Call(), TEXT("ReturnSet should execute through reflection")));
		ReturnSlot = ReturnSetParam->ContainerPtrToValuePtr<void>(ReturnSetInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TSet<int> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptSetHelper ReturnSetHelper(ReturnSetParam, ReturnSlot);
		ASSERT_THAT(AreEqual(2, ReturnSetHelper.Num(), TEXT("TSet<int> return should contain two entries")));
		ASSERT_THAT(IsTrue(SetContainsIntValue(*TestRunner, *ReturnSetParam, ReturnSlot, 37),
			TEXT("TSet<int> return should preserve set values")));
	}

	TEST_METHOD(AdvancedFunctionMetadataAndRuntimeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_AdvancedMetadataRuntime"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUFunctionAdvancedMetadataActor : AActor
			{
				UPROPERTY()
				int LastScore = 0;

				UFUNCTION(BlueprintCallable, BlueprintProtected, Category="Coverage|AdvancedMeta", meta=(ScriptName="CoverageRenamedAction", DeprecatedFunction, DeprecationMessage="Use ReplacementAction", DevelopmentOnly, BlueprintInternalUseOnly, DefaultToSelf="Target", HidePin="Target", AutoCreateRefTerm="Label", DeterminesOutputType="RequestedClass", ExpandEnumAsExecs="ExecResult", CustomCoverageKey="CustomValue"))
				int MetadataRichAction(UObject Target, UClass RequestedClass, FString Label, int RequiredValue, int OptionalValue = 5)
				{
					LastScore = (Target != nullptr ? 1 : 0)
						+ (RequestedClass != nullptr ? 2 : 0)
						+ Label.Len()
						+ RequiredValue
						+ OptionalValue;
					return LastScore;
				}

				UFUNCTION(BlueprintCallable, meta=(EditorOnly))
				void EditorOnlyMetaAction()
				{
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionAdvancedMetadataRuntime.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionAdvancedMetadataActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("advanced metadata UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* MetadataRichAction = FindFunctionForTest(ScriptClass, TEXT("MetadataRichAction"));
		UFunction* EditorOnlyMetaAction = FindFunctionForTest(ScriptClass, TEXT("EditorOnlyMetaAction"));
		ASSERT_THAT(IsNotNull(MetadataRichAction, TEXT("metadata-rich UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(EditorOnlyMetaAction, TEXT("EditorOnly metadata UFUNCTION should be generated")));
		if (MetadataRichAction == nullptr || EditorOnlyMetaAction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(MetadataRichAction->HasAnyFunctionFlags(FUNC_BlueprintCallable),
			TEXT("metadata-rich UFUNCTION should remain BlueprintCallable")));
		ASSERT_THAT(IsTrue(MetadataRichAction->HasMetaData(TEXT("BlueprintProtected")),
			TEXT("BlueprintProtected should round-trip as UFunction metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|AdvancedMeta")), MetadataRichAction->GetMetaData(TEXT("Category")),
			TEXT("advanced UFUNCTION Category metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("CoverageRenamedAction")), MetadataRichAction->GetMetaData(TEXT("ScriptName")),
			TEXT("ScriptName metadata should be preserved without renaming the generated UFunction")));
		ASSERT_THAT(IsTrue(MetadataRichAction->HasMetaData(TEXT("DeprecatedFunction")),
			TEXT("DeprecatedFunction metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("Use ReplacementAction")), MetadataRichAction->GetMetaData(TEXT("DeprecationMessage")),
			TEXT("DeprecationMessage metadata should be preserved")));
		ASSERT_THAT(IsTrue(MetadataRichAction->HasMetaData(TEXT("DevelopmentOnly")),
			TEXT("DevelopmentOnly metadata should round-trip through meta")));
		ASSERT_THAT(IsTrue(MetadataRichAction->HasMetaData(TEXT("BlueprintInternalUseOnly")),
			TEXT("BlueprintInternalUseOnly metadata should round-trip through meta")));
		ASSERT_THAT(AreEqual(FString(TEXT("Target")), MetadataRichAction->GetMetaData(TEXT("DefaultToSelf")),
			TEXT("DefaultToSelf metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("Target")), MetadataRichAction->GetMetaData(TEXT("HidePin")),
			TEXT("HidePin metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("Label")), MetadataRichAction->GetMetaData(TEXT("AutoCreateRefTerm")),
			TEXT("AutoCreateRefTerm metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("RequestedClass")), MetadataRichAction->GetMetaData(TEXT("DeterminesOutputType")),
			TEXT("DeterminesOutputType metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("ExecResult")), MetadataRichAction->GetMetaData(TEXT("ExpandEnumAsExecs")),
			TEXT("ExpandEnumAsExecs metadata should be preserved")));
		ASSERT_THAT(AreEqual(FString(TEXT("CustomValue")), MetadataRichAction->GetMetaData(TEXT("CustomCoverageKey")),
			TEXT("custom UFUNCTION metadata should be preserved")));
		ASSERT_THAT(IsTrue(EditorOnlyMetaAction->HasAnyFunctionFlags(FUNC_EditorOnly),
			TEXT("meta=(EditorOnly) should set FUNC_EditorOnly on UFunctions")));

		FObjectProperty* TargetParam = CastField<FObjectProperty>(FindParameterForTest(MetadataRichAction, TEXT("Target")));
		FClassProperty* RequestedClassParam = CastField<FClassProperty>(FindParameterForTest(MetadataRichAction, TEXT("RequestedClass")));
		FStrProperty* LabelParam = CastField<FStrProperty>(FindParameterForTest(MetadataRichAction, TEXT("Label")));
		FIntProperty* RequiredParam = CastField<FIntProperty>(FindParameterForTest(MetadataRichAction, TEXT("RequiredValue")));
		FIntProperty* OptionalParam = CastField<FIntProperty>(FindParameterForTest(MetadataRichAction, TEXT("OptionalValue")));
		FIntProperty* ReturnParam = CastField<FIntProperty>(FindParameterForTest(MetadataRichAction, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(TargetParam, TEXT("DefaultToSelf target parameter should reflect as FObjectProperty")));
		ASSERT_THAT(IsNotNull(RequestedClassParam, TEXT("DeterminesOutputType parameter should reflect as FClassProperty")));
		ASSERT_THAT(IsNotNull(LabelParam, TEXT("AutoCreateRefTerm FString parameter should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(RequiredParam, TEXT("required int parameter should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(OptionalParam, TEXT("defaulted int parameter should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(ReturnParam, TEXT("metadata-rich return should reflect as FIntProperty")));
		if (TargetParam == nullptr || RequestedClassParam == nullptr || LabelParam == nullptr
			|| RequiredParam == nullptr || OptionalParam == nullptr || ReturnParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(UObject::StaticClass(), TargetParam->PropertyClass,
			TEXT("DefaultToSelf target parameter should preserve UObject class")));
		ASSERT_THAT(AreEqual(UObject::StaticClass(), RequestedClassParam->MetaClass,
			TEXT("DeterminesOutputType UClass parameter should allow UObject classes")));
		ASSERT_THAT(AreEqual(FString(TEXT("5")), MetadataRichAction->GetMetaData(TEXT("CPP_Default_OptionalValue")),
			TEXT("metadata-rich UFUNCTION default parameter should round-trip")));
		ASSERT_THAT(IsTrue(ReturnParam->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("metadata-rich return should carry CPF_ReturnParm")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("advanced metadata UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("MetadataRichAction"));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("MetadataRichAction should be invokable")));
		if (!Invoker.IsValid())
		{
			return;
		}
		Invoker.AddParam<UObject*>(Actor);
		Invoker.AddParam<UClass*>(ScriptClass);
		Invoker.AddParam<FString>(FString(TEXT("Meta")));
		Invoker.AddParam<int32>(30);
		Invoker.AddParam<int32>(5);
		ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("metadata-rich UFUNCTION should execute through reflection")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastScore"), 42,
			TEXT("metadata-rich UFUNCTION should update reflected state"))));
	}

	TEST_METHOD(ScriptStructParameterDirectionAndReturnMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_StructParameterDirections"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FUFunctionDirectionPayload
			{
				UPROPERTY()
				int Count = 0;

				UPROPERTY()
				FString Label;
			}

			UCLASS()
			class ACoverageUFunctionStructDirectionActor : AActor
			{
				UPROPERTY()
				FUFunctionDirectionPayload LastValue;

				UPROPERTY()
				FUFunctionDirectionPayload LastConstRef;

				UPROPERTY()
				bool bInoutSawOriginal = false;

				UFUNCTION(BlueprintCallable, Category="Coverage|StructDirections")
				int AcceptValue(FUFunctionDirectionPayload Payload)
				{
					LastValue = Payload;
					return Payload.Count + Payload.Label.Len();
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|StructDirections")
				int AcceptConstRef(const FUFunctionDirectionPayload&in Payload)
				{
					LastConstRef = Payload;
					return Payload.Count + Payload.Label.Len();
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|StructDirections")
				void FillOut(FUFunctionDirectionPayload&out Payload)
				{
					Payload.Count = 42;
					Payload.Label = "OutPayload";
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|StructDirections")
				int MutateInout(FUFunctionDirectionPayload&inout Payload)
				{
					bInoutSawOriginal = Payload.Count == 10 && Payload.Label == "Input";
					Payload.Count += 19;
					Payload.Label += "|Mutated";
					return Payload.Count + Payload.Label.Len();
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|StructDirections")
				FUFunctionDirectionPayload ReturnPayload()
				{
					FUFunctionDirectionPayload Payload;
					Payload.Count = 42;
					Payload.Label = "ReturnPayload";
					return Payload;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionStructParameterDirections.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionStructDirectionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("struct-direction UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* AcceptValue = FindFunctionForTest(ScriptClass, TEXT("AcceptValue"));
		UFunction* AcceptConstRef = FindFunctionForTest(ScriptClass, TEXT("AcceptConstRef"));
		UFunction* FillOut = FindFunctionForTest(ScriptClass, TEXT("FillOut"));
		UFunction* MutateInout = FindFunctionForTest(ScriptClass, TEXT("MutateInout"));
		UFunction* ReturnPayload = FindFunctionForTest(ScriptClass, TEXT("ReturnPayload"));
		ASSERT_THAT(IsNotNull(AcceptValue, TEXT("struct value UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(AcceptConstRef, TEXT("struct const-ref UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(FillOut, TEXT("struct out UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(MutateInout, TEXT("struct inout UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnPayload, TEXT("struct return UFUNCTION should be generated")));
		if (AcceptValue == nullptr || AcceptConstRef == nullptr || FillOut == nullptr || MutateInout == nullptr || ReturnPayload == nullptr)
		{
			return;
		}

		FStructProperty* ValueParam = CastField<FStructProperty>(FindParameterForTest(AcceptValue, TEXT("Payload")));
		FStructProperty* ConstRefParam = CastField<FStructProperty>(FindParameterForTest(AcceptConstRef, TEXT("Payload")));
		FStructProperty* OutParam = CastField<FStructProperty>(FindParameterForTest(FillOut, TEXT("Payload")));
		FStructProperty* InoutParam = CastField<FStructProperty>(FindParameterForTest(MutateInout, TEXT("Payload")));
		FStructProperty* ReturnParam = CastField<FStructProperty>(FindParameterForTest(ReturnPayload, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(ValueParam, TEXT("AS USTRUCT value parameter should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(ConstRefParam, TEXT("AS USTRUCT const-ref parameter should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(OutParam, TEXT("AS USTRUCT out parameter should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(InoutParam, TEXT("AS USTRUCT inout parameter should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(ReturnParam, TEXT("AS USTRUCT return should reflect as FStructProperty")));
		if (ValueParam == nullptr || ConstRefParam == nullptr || OutParam == nullptr || InoutParam == nullptr || ReturnParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(ValueParam->Struct, ConstRefParam->Struct,
			TEXT("value and const-ref struct parameters should use the same generated UScriptStruct")));
		ASSERT_THAT(AreEqual(ValueParam->Struct, OutParam->Struct,
			TEXT("value and out struct parameters should use the same generated UScriptStruct")));
		ASSERT_THAT(AreEqual(ValueParam->Struct, InoutParam->Struct,
			TEXT("value and inout struct parameters should use the same generated UScriptStruct")));
		ASSERT_THAT(AreEqual(ValueParam->Struct, ReturnParam->Struct,
			TEXT("value parameter and return should use the same generated UScriptStruct")));
		ASSERT_THAT(IsFalse(ValueParam->HasAnyPropertyFlags(CPF_OutParm | CPF_ConstParm | CPF_ReturnParm),
			TEXT("AS USTRUCT value parameter should remain a plain input")));
		ASSERT_THAT(IsTrue(ConstRefParam->HasAllPropertyFlags(CPF_ConstParm | CPF_OutParm | CPF_ReferenceParm),
			TEXT("AS USTRUCT const-ref parameter should carry const/out/reference flags")));
		ASSERT_THAT(IsTrue(OutParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("AS USTRUCT out parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsFalse(OutParam->HasAnyPropertyFlags(CPF_ConstParm | CPF_ReturnParm),
			TEXT("AS USTRUCT out parameter should not carry const or return flags")));
		ASSERT_THAT(IsTrue(InoutParam->HasAllPropertyFlags(CPF_OutParm | CPF_ReferenceParm),
			TEXT("AS USTRUCT inout parameter should carry out/reference flags")));
		ASSERT_THAT(IsTrue(ReturnParam->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("AS USTRUCT return should carry CPF_ReturnParm")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("struct-direction UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker ValueInvoker(*TestRunner, Actor, TEXT("AcceptValue"));
		ASSERT_THAT(IsTrue(ValueInvoker.IsValid(), TEXT("AcceptValue should be invokable")));
		if (!ValueInvoker.IsValid())
		{
			return;
		}
		FProperty* ParamProperty = nullptr;
		void* ParamSlot = nullptr;
		ASSERT_THAT(IsTrue(ValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("AcceptValue should expose a struct value parameter slot")));
		FStructProperty* RuntimeValueParam = CastField<FStructProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(RuntimeValueParam, TEXT("AcceptValue runtime slot should be FStructProperty")));
		if (ParamSlot == nullptr || RuntimeValueParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(WritePayloadStructValue(*TestRunner, *RuntimeValueParam, ParamSlot, 37, FString(TEXT("Value")))));
		ASSERT_THAT(AreEqual(42, ValueInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("AS USTRUCT value parameter should execute through reflection")));

		FFunctionInvoker ConstRefInvoker(*TestRunner, Actor, TEXT("AcceptConstRef"));
		ASSERT_THAT(IsTrue(ConstRefInvoker.IsValid(), TEXT("AcceptConstRef should be invokable")));
		if (!ConstRefInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ConstRefInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("AcceptConstRef should expose a struct const-ref parameter slot")));
		FStructProperty* RuntimeConstRefParam = CastField<FStructProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(RuntimeConstRefParam, TEXT("AcceptConstRef runtime slot should be FStructProperty")));
		if (ParamSlot == nullptr || RuntimeConstRefParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(WritePayloadStructValue(*TestRunner, *RuntimeConstRefParam, ParamSlot, 34, FString(TEXT("ConstRef")))));
		ASSERT_THAT(AreEqual(42, ConstRefInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("AS USTRUCT const-ref parameter should execute through reflection")));

		FFunctionInvoker OutInvoker(*TestRunner, Actor, TEXT("FillOut"));
		ASSERT_THAT(IsTrue(OutInvoker.IsValid(), TEXT("FillOut should be invokable")));
		if (!OutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(OutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillOut should expose a struct out parameter slot")));
		FStructProperty* RuntimeOutParam = CastField<FStructProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(RuntimeOutParam, TEXT("FillOut runtime slot should be FStructProperty")));
		if (ParamSlot == nullptr || RuntimeOutParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(OutInvoker.Call(), TEXT("AS USTRUCT out parameter should execute through reflection")));
		ASSERT_THAT(IsTrue(VerifyPayloadStructValue(*TestRunner, *RuntimeOutParam, ParamSlot, 42, FString(TEXT("OutPayload")),
			TEXT("AS USTRUCT out parameter"))));

		FFunctionInvoker InoutInvoker(*TestRunner, Actor, TEXT("MutateInout"));
		ASSERT_THAT(IsTrue(InoutInvoker.IsValid(), TEXT("MutateInout should be invokable")));
		if (!InoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(InoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateInout should expose a struct inout parameter slot")));
		FStructProperty* RuntimeInoutParam = CastField<FStructProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(RuntimeInoutParam, TEXT("MutateInout runtime slot should be FStructProperty")));
		if (ParamSlot == nullptr || RuntimeInoutParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(WritePayloadStructValue(*TestRunner, *RuntimeInoutParam, ParamSlot, 10, FString(TEXT("Input")))));
		ASSERT_THAT(AreEqual(42, InoutInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("AS USTRUCT inout parameter should execute through reflection")));
		ASSERT_THAT(IsTrue(VerifyPayloadStructValue(*TestRunner, *RuntimeInoutParam, ParamSlot, 29, FString(TEXT("Input|Mutated")),
			TEXT("AS USTRUCT inout parameter"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bInoutSawOriginal"), true,
			TEXT("AS USTRUCT inout parameter should read caller-provided fields before mutation"))));

		FFunctionInvoker ReturnInvoker(*TestRunner, Actor, TEXT("ReturnPayload"));
		ASSERT_THAT(IsTrue(ReturnInvoker.IsValid(), TEXT("ReturnPayload should be invokable")));
		if (!ReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ReturnInvoker.Call(), TEXT("AS USTRUCT return should execute through reflection")));
		void* ReturnSlot = ReturnParam->ContainerPtrToValuePtr<void>(ReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("AS USTRUCT return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VerifyPayloadStructValue(*TestRunner, *ReturnParam, ReturnSlot, 42, FString(TEXT("ReturnPayload")),
			TEXT("AS USTRUCT return"))));
	}

	TEST_METHOD(OptionalReturnReflectionMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_OptionalReturnMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FUFunctionOptionalPayload
			{
				UPROPERTY()
				int Count = 0;

				UPROPERTY()
				FString Label;
			}

			UCLASS()
			class ACoverageUFunctionOptionalReturnActor : AActor
			{
				UPROPERTY()
				bool bSetIntObserved = false;

				UPROPERTY()
				bool bEmptyIntObserved = false;

				UPROPERTY()
				bool bSetPayloadObserved = false;

				UPROPERTY()
				bool bEmptyPayloadObserved = false;

				UFUNCTION(BlueprintCallable, Category="Coverage|Optional")
				TOptional<int> ReturnSetInt()
				{
					TOptional<int> Result;
					Result.Set(42);
					bSetIntObserved = Result.IsSet() && Result.GetValue() == 42;
					return Result;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Optional")
				TOptional<int> ReturnEmptyInt()
				{
					TOptional<int> Result;
					bEmptyIntObserved = !Result.IsSet();
					return Result;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Optional")
				TOptional<FUFunctionOptionalPayload> ReturnSetPayload()
				{
					FUFunctionOptionalPayload Payload;
					Payload.Count = 42;
					Payload.Label = "OptionalPayload";

					TOptional<FUFunctionOptionalPayload> Result;
					Result.Set(Payload);
					bSetPayloadObserved = Result.IsSet()
						&& Result.GetValue().Count == 42
						&& Result.GetValue().Label == "OptionalPayload";
					return Result;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|Optional")
				TOptional<FUFunctionOptionalPayload> ReturnEmptyPayload()
				{
					TOptional<FUFunctionOptionalPayload> Result;
					bEmptyPayloadObserved = !Result.IsSet();
					return Result;
				}
			}
			)AS");
		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionOptionalReturnMatrix.as"),
			ScriptSource,
			TEXT("ACoverageUFunctionOptionalReturnActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("optional-return UFUNCTION actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* ReturnSetInt = FindFunctionForTest(ScriptClass, TEXT("ReturnSetInt"));
		UFunction* ReturnEmptyInt = FindFunctionForTest(ScriptClass, TEXT("ReturnEmptyInt"));
		UFunction* ReturnSetPayload = FindFunctionForTest(ScriptClass, TEXT("ReturnSetPayload"));
		UFunction* ReturnEmptyPayload = FindFunctionForTest(ScriptClass, TEXT("ReturnEmptyPayload"));
		ASSERT_THAT(IsNotNull(ReturnSetInt, TEXT("TOptional<int> set return UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnEmptyInt, TEXT("TOptional<int> empty return UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnSetPayload, TEXT("TOptional<USTRUCT> set return UFUNCTION should be generated")));
		ASSERT_THAT(IsNotNull(ReturnEmptyPayload, TEXT("TOptional<USTRUCT> empty return UFUNCTION should be generated")));
		if (ReturnSetInt == nullptr || ReturnEmptyInt == nullptr || ReturnSetPayload == nullptr || ReturnEmptyPayload == nullptr)
		{
			return;
		}

		FOptionalProperty* SetIntReturn = CastField<FOptionalProperty>(ReturnSetInt->GetReturnProperty());
		FOptionalProperty* EmptyIntReturn = CastField<FOptionalProperty>(ReturnEmptyInt->GetReturnProperty());
		FOptionalProperty* SetPayloadReturn = CastField<FOptionalProperty>(ReturnSetPayload->GetReturnProperty());
		FOptionalProperty* EmptyPayloadReturn = CastField<FOptionalProperty>(ReturnEmptyPayload->GetReturnProperty());
		ASSERT_THAT(IsNotNull(SetIntReturn, TEXT("TOptional<int> set return should reflect as FOptionalProperty")));
		ASSERT_THAT(IsNotNull(EmptyIntReturn, TEXT("TOptional<int> empty return should reflect as FOptionalProperty")));
		ASSERT_THAT(IsNotNull(SetPayloadReturn, TEXT("TOptional<USTRUCT> set return should reflect as FOptionalProperty")));
		ASSERT_THAT(IsNotNull(EmptyPayloadReturn, TEXT("TOptional<USTRUCT> empty return should reflect as FOptionalProperty")));
		if (SetIntReturn == nullptr || EmptyIntReturn == nullptr || SetPayloadReturn == nullptr || EmptyPayloadReturn == nullptr)
		{
			return;
		}

		FIntProperty* SetIntInner = CastField<FIntProperty>(SetIntReturn->GetValueProperty());
		FIntProperty* EmptyIntInner = CastField<FIntProperty>(EmptyIntReturn->GetValueProperty());
		FStructProperty* SetPayloadInner = CastField<FStructProperty>(SetPayloadReturn->GetValueProperty());
		FStructProperty* EmptyPayloadInner = CastField<FStructProperty>(EmptyPayloadReturn->GetValueProperty());
		ASSERT_THAT(IsNotNull(SetIntInner, TEXT("TOptional<int> set return inner should be FIntProperty")));
		ASSERT_THAT(IsNotNull(EmptyIntInner, TEXT("TOptional<int> empty return inner should be FIntProperty")));
		ASSERT_THAT(IsNotNull(SetPayloadInner, TEXT("TOptional<USTRUCT> set return inner should be FStructProperty")));
		ASSERT_THAT(IsNotNull(EmptyPayloadInner, TEXT("TOptional<USTRUCT> empty return inner should be FStructProperty")));
		if (SetIntInner == nullptr || EmptyIntInner == nullptr || SetPayloadInner == nullptr || EmptyPayloadInner == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(SetPayloadInner->Struct, EmptyPayloadInner->Struct,
			TEXT("set and empty TOptional<USTRUCT> returns should use the same generated UScriptStruct")));
		ASSERT_THAT(IsTrue(SetIntReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("TOptional<int> return should carry CPF_ReturnParm")));
		ASSERT_THAT(IsTrue(SetPayloadReturn->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("TOptional<USTRUCT> return should carry CPF_ReturnParm")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("optional-return UFUNCTION actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker SetIntInvoker(*TestRunner, Actor, TEXT("ReturnSetInt"));
		ASSERT_THAT(IsTrue(SetIntInvoker.IsValid(), TEXT("ReturnSetInt should be invokable")));
		if (!SetIntInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetIntInvoker.Call(), TEXT("ReturnSetInt should execute through reflection")));
		void* OptionalSlot = SetIntReturn->ContainerPtrToValuePtr<void>(SetIntInvoker.GetParamsMemory());
		ASSERT_THAT(IsTrue(SetIntReturn->IsSet(OptionalSlot), TEXT("TOptional<int> set return should report IsSet=true")));
		ASSERT_THAT(AreEqual(42, SetIntInner->GetPropertyValue(SetIntReturn->GetValuePointerForRead(OptionalSlot)),
			TEXT("TOptional<int> set return should preserve inner value")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSetIntObserved"), true,
			TEXT("TOptional<int> set return should be observable in AS"))));

		FFunctionInvoker EmptyIntInvoker(*TestRunner, Actor, TEXT("ReturnEmptyInt"));
		ASSERT_THAT(IsTrue(EmptyIntInvoker.IsValid(), TEXT("ReturnEmptyInt should be invokable")));
		if (!EmptyIntInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(EmptyIntInvoker.Call(), TEXT("ReturnEmptyInt should execute through reflection")));
		OptionalSlot = EmptyIntReturn->ContainerPtrToValuePtr<void>(EmptyIntInvoker.GetParamsMemory());
		ASSERT_THAT(IsFalse(EmptyIntReturn->IsSet(OptionalSlot), TEXT("TOptional<int> empty return should report IsSet=false")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bEmptyIntObserved"), true,
			TEXT("TOptional<int> empty return should be observable in AS"))));

		FFunctionInvoker SetPayloadInvoker(*TestRunner, Actor, TEXT("ReturnSetPayload"));
		ASSERT_THAT(IsTrue(SetPayloadInvoker.IsValid(), TEXT("ReturnSetPayload should be invokable")));
		if (!SetPayloadInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetPayloadInvoker.Call(), TEXT("ReturnSetPayload should execute through reflection")));
		OptionalSlot = SetPayloadReturn->ContainerPtrToValuePtr<void>(SetPayloadInvoker.GetParamsMemory());
		ASSERT_THAT(IsTrue(SetPayloadReturn->IsSet(OptionalSlot), TEXT("TOptional<USTRUCT> set return should report IsSet=true")));
		const void* InnerPayloadAddress = SetPayloadReturn->GetValuePointerForRead(OptionalSlot);
		ASSERT_THAT(IsNotNull(InnerPayloadAddress, TEXT("TOptional<USTRUCT> set return should expose inner payload memory")));
		if (InnerPayloadAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VerifyPayloadStructValue(*TestRunner, *SetPayloadInner, InnerPayloadAddress, 42, FString(TEXT("OptionalPayload")),
			TEXT("TOptional<USTRUCT> set return"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSetPayloadObserved"), true,
			TEXT("TOptional<USTRUCT> set return should be observable in AS"))));

		FFunctionInvoker EmptyPayloadInvoker(*TestRunner, Actor, TEXT("ReturnEmptyPayload"));
		ASSERT_THAT(IsTrue(EmptyPayloadInvoker.IsValid(), TEXT("ReturnEmptyPayload should be invokable")));
		if (!EmptyPayloadInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(EmptyPayloadInvoker.Call(), TEXT("ReturnEmptyPayload should execute through reflection")));
		OptionalSlot = EmptyPayloadReturn->ContainerPtrToValuePtr<void>(EmptyPayloadInvoker.GetParamsMemory());
		ASSERT_THAT(IsFalse(EmptyPayloadReturn->IsSet(OptionalSlot), TEXT("TOptional<USTRUCT> empty return should report IsSet=false")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bEmptyPayloadObserved"), true,
			TEXT("TOptional<USTRUCT> empty return should be observable in AS"))));
	}

	TEST_METHOD(UnsupportedUFunctionShapeDiagnostics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		struct FInvalidUFunctionCase
		{
			const TCHAR* Label;
			FName ModuleName;
			const TCHAR* Filename;
			FString ScriptSource;
			const TCHAR* ExpectedDiagnostic;
		};

		const FInvalidUFunctionCase Cases[] = {
			{
				TEXT("global BlueprintEvent"),
				TEXT("ASCoverageUFunction_InvalidGlobalBlueprintEvent"),
				TEXT("ASCoverageUFunctionInvalidGlobalBlueprintEvent.as"),
				ASTEST_AS(R"AS(
				UFUNCTION(BlueprintEvent)
				int BadGlobalEvent()
				{
					return 1;
				}
				)AS"),
				TEXT("Global UFUNCTION() BadGlobalEvent may not be marked BlueprintEvent.")
			},
			{
				TEXT("global BlueprintOverride"),
				TEXT("ASCoverageUFunction_InvalidGlobalBlueprintOverride"),
				TEXT("ASCoverageUFunctionInvalidGlobalBlueprintOverride.as"),
				ASTEST_AS(R"AS(
				UFUNCTION(BlueprintOverride)
				void BadGlobalOverride()
				{
				}
				)AS"),
				TEXT("Global UFUNCTION() BadGlobalOverride may not be BlueprintOverride.")
			},
			{
				TEXT("USTRUCT member UFUNCTION"),
				TEXT("ASCoverageUFunction_InvalidStructMemberFunction"),
				TEXT("ASCoverageUFunctionInvalidStructMemberFunction.as"),
				ASTEST_AS(R"AS(
				USTRUCT()
				struct FBadFunctionStruct
				{
					UFUNCTION()
					int BadMember()
					{
						return 1;
					}
				}
				)AS"),
				TEXT("Structs may not have any UFUNCTION()s.")
			},
			{
				TEXT("BlueprintEvent and BlueprintOverride conflict"),
				TEXT("ASCoverageUFunction_InvalidEventOverrideConflict"),
				TEXT("ASCoverageUFunctionInvalidEventOverrideConflict.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionConflictActor : AActor
				{
					UFUNCTION(BlueprintEvent, BlueprintOverride)
					void Conflict()
					{
					}
				}
				)AS"),
				TEXT("UFUNCTION() Conflict cannot be both BlueprintEvent and BlueprintOverride.")
			},
			{
				TEXT("network BlueprintOverride conflict"),
				TEXT("ASCoverageUFunction_InvalidNetBlueprintOverride"),
				TEXT("ASCoverageUFunctionInvalidNetBlueprintOverride.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionNetOverrideConflictActor : AActor
				{
					UFUNCTION(Server, BlueprintOverride)
					void Conflict()
					{
					}
				}
				)AS"),
				TEXT("UFUNCTION() Conflict cannot both be BlueprintOverride and have network specifiers")
			},
			{
				TEXT("WithValidation without RPC endpoint"),
				TEXT("ASCoverageUFunction_InvalidValidationWithoutEndpoint"),
				TEXT("ASCoverageUFunctionInvalidValidationWithoutEndpoint.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionValidationWithoutEndpointActor : AActor
				{
					UFUNCTION(WithValidation)
					void ValidateOnly()
					{
					}
				}
				)AS"),
				TEXT("UFUNCTION() ValidateOnly has the WithValidation property without the Server or Client property!")
			},
			{
				TEXT("static network specifier"),
				TEXT("ASCoverageUFunction_InvalidStaticNetworkSpecifier"),
				TEXT("ASCoverageUFunctionInvalidStaticNetworkSpecifier.as"),
				ASTEST_AS(R"AS(
				UFUNCTION(Server)
				void StaticServerAction()
				{
				}
				)AS"),
				TEXT("Static UFUNCTION()s cannot use network specifiers")
			},
			{
				TEXT("BlueprintPure without return or out parameter"),
				TEXT("ASCoverageUFunction_InvalidPureWithoutResult"),
				TEXT("ASCoverageUFunctionInvalidPureWithoutResult.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionPureWithoutResultActor : AActor
				{
					UFUNCTION(BlueprintPure)
					void PureWithoutResult()
					{
					}
				}
				)AS"),
				TEXT("BlueprintPure method PureWithoutResult in class ACoverageUFunctionPureWithoutResultActor must have return value.")
			},
			{
				TEXT("BlueprintOverride missing parent event"),
				TEXT("ASCoverageUFunction_InvalidMissingOverrideParent"),
				TEXT("ASCoverageUFunctionInvalidMissingOverrideParent.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionMissingOverrideBaseActor : AActor
				{
				}

				UCLASS()
				class ACoverageUFunctionMissingOverrideChildActor : ACoverageUFunctionMissingOverrideBaseActor
				{
					UFUNCTION(BlueprintOverride)
					void MissingOverride()
					{
					}
				}
				)AS"),
				TEXT("BlueprintOverride method MissingOverride in class ACoverageUFunctionMissingOverrideChildActor does not exist in superclass ACoverageUFunctionMissingOverrideBaseActor.")
			},
			{
				TEXT("BlueprintOverride parent is not event"),
				TEXT("ASCoverageUFunction_InvalidOverrideNonEventParent"),
				TEXT("ASCoverageUFunctionInvalidOverrideNonEventParent.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionNonEventBaseActor : AActor
				{
					UFUNCTION(BlueprintCallable)
					void NotAnEvent()
					{
					}
				}

				UCLASS()
				class ACoverageUFunctionNonEventChildActor : ACoverageUFunctionNonEventBaseActor
				{
					UFUNCTION(BlueprintOverride)
					void NotAnEvent()
					{
					}
				}
				)AS"),
				TEXT("BlueprintOverride method NotAnEvent in class ACoverageUFunctionNonEventChildActor is not marked BlueprintEvent in superclass ACoverageUFunctionNonEventBaseActor.")
			},
			{
				TEXT("BlueprintCallable parent signature mismatch"),
				TEXT("ASCoverageUFunction_InvalidCallableParentSignature"),
				TEXT("ASCoverageUFunctionInvalidCallableParentSignature.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionCallableMismatchBaseActor : AActor
				{
					UFUNCTION(BlueprintCallable)
					int ComputeValue(int Value)
					{
						return Value;
					}
				}

				UCLASS()
				class ACoverageUFunctionCallableMismatchChildActor : ACoverageUFunctionCallableMismatchBaseActor
				{
					UFUNCTION(BlueprintCallable)
					int ComputeValue(FString Value)
					{
						return Value.Len();
					}
				}
				)AS"),
				TEXT("BlueprintCallable method ComputeValue in class ACoverageUFunctionCallableMismatchChildActor is specified in superclass ACoverageUFunctionCallableMismatchBaseActor with a different signature.")
			},
			{
				TEXT("duplicate UFUNCTION name overload"),
				TEXT("ASCoverageUFunction_InvalidDuplicateName"),
				TEXT("ASCoverageUFunctionInvalidDuplicateName.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionDuplicateNameActor : AActor
				{
					UFUNCTION()
					void Duplicate()
					{
					}

					UFUNCTION()
					void Duplicate(int Value)
					{
					}
				}
				)AS"),
				TEXT("Multiple methods with name Duplicate in class ACoverageUFunctionDuplicateNameActor found. UFUNCTION()s must have unique names.")
			},
			{
				TEXT("UFUNCTION return reference unsupported"),
				TEXT("ASCoverageUFunction_InvalidReferenceReturn"),
				TEXT("ASCoverageUFunctionInvalidReferenceReturn.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionReferenceReturnActor : AActor
				{
					UPROPERTY()
					int StoredValue = 7;

					UFUNCTION()
					int& ReturnStoredValueRef()
					{
						return StoredValue;
					}
				}
				)AS"),
				TEXT("UFUNCTIONs cannot return references, function ReturnStoredValueRef in class ACoverageUFunctionReferenceReturnActor")
			},
			{
				TEXT("BlueprintOverride signature mismatch"),
				TEXT("ASCoverageUFunction_InvalidOverrideSignature"),
				TEXT("ASCoverageUFunctionInvalidOverrideSignature.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionMismatchBaseActor : AActor
				{
					UFUNCTION(BlueprintEvent)
					int ComputeValue(int Value)
					{
						return Value + 1;
					}
				}

				UCLASS()
				class ACoverageUFunctionMismatchChildActor : ACoverageUFunctionMismatchBaseActor
				{
					UFUNCTION(BlueprintOverride)
					int ComputeValue(FString Value)
					{
						return Value.Len();
					}
				}
				)AS"),
				TEXT("BlueprintOverride method ComputeValue in class ACoverageUFunctionMismatchChildActor does not match signature of event declared in superclass ACoverageUFunctionMismatchBaseActor.")
			},
			{
				TEXT("WithValidation missing companion"),
				TEXT("ASCoverageUFunction_InvalidMissingValidateCompanion"),
				TEXT("ASCoverageUFunctionInvalidMissingValidateCompanion.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionMissingValidateActor : AActor
				{
					default SetReplicates(true);

					UFUNCTION(Server, WithValidation)
					void ServerMissingValidate(int Value)
					{
					}
				}
				)AS"),
				TEXT("UFUNCTION() ServerMissingValidate in class ACoverageUFunctionMissingValidateActor is marked as WithValidate but no _Validate function provided! Is it marked as UFUNCTION()?")
			},
			{
				TEXT("WithValidation non-bool companion"),
				TEXT("ASCoverageUFunction_InvalidValidateReturn"),
				TEXT("ASCoverageUFunctionInvalidValidateReturn.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionBadValidateReturnActor : AActor
				{
					default SetReplicates(true);

					UFUNCTION(Server, WithValidation)
					void ServerBadValidateReturn(int Value)
					{
					}

					UFUNCTION()
					int ServerBadValidateReturn_Validate(int Value)
					{
						return Value;
					}
				}
				)AS"),
				TEXT("UFUNCTION() ServerBadValidateReturn in class ACoverageUFunctionBadValidateReturnActor has a _Validate function that is returning a non-bool!")
			},
			{
				TEXT("WithValidation companion parameter mismatch"),
				TEXT("ASCoverageUFunction_InvalidValidateParameters"),
				TEXT("ASCoverageUFunctionInvalidValidateParameters.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionBadValidateParamsActor : AActor
				{
					default SetReplicates(true);

					UFUNCTION(Server, WithValidation)
					void ServerBadValidateParams(int Value)
					{
					}

					UFUNCTION()
					bool ServerBadValidateParams_Validate(FString Value)
					{
						return !Value.IsEmpty();
					}
				}
				)AS"),
				TEXT("UFUNCTION() ServerBadValidateParams in class ACoverageUFunctionBadValidateParamsActor has a _Validate function but the parameters don't match!")
			},
			{
				TEXT("unknown function specifier"),
				TEXT("ASCoverageUFunction_InvalidUnknownSpecifier"),
				TEXT("ASCoverageUFunctionInvalidUnknownSpecifier.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionUnknownSpecifierActor : AActor
				{
					UFUNCTION(DefinitelyUnknownSpecifier)
					void UnknownSpecifier()
					{
					}
				}
				)AS"),
				TEXT("Unknown function specifier DefinitelyUnknownSpecifier on method ACoverageUFunctionUnknownSpecifierActor::UnknownSpecifier.")
			},
			{
				TEXT("BlueprintEvent already specified in AS superclass"),
				TEXT("ASCoverageUFunction_InvalidDuplicateBlueprintEventParent"),
				TEXT("ASCoverageUFunctionInvalidDuplicateBlueprintEventParent.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionParentEventBaseActor : AActor
				{
					UFUNCTION(BlueprintEvent)
					int ComputeParentEvent(int Value)
					{
						return Value + 1;
					}
				}

				UCLASS()
				class ACoverageUFunctionParentEventChildActor : ACoverageUFunctionParentEventBaseActor
				{
					UFUNCTION(BlueprintEvent)
					int ComputeParentEvent(int Value)
					{
						return Value + 2;
					}
				}
				)AS"),
				TEXT("BlueprintEvent method ComputeParentEvent in class ACoverageUFunctionParentEventChildActor is already specified in superclass ACoverageUFunctionParentEventBaseActor.")
			},
			{
				TEXT("BlueprintCallable collides with native non-event"),
				TEXT("ASCoverageUFunction_InvalidNativeCallableCollision"),
				TEXT("ASCoverageUFunctionInvalidNativeCallableCollision.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionNativeCollisionActor : AActor
				{
					UFUNCTION(BlueprintCallable)
					void SetActorHiddenInGame(bool bNewHidden)
					{
					}
				}
				)AS"),
				TEXT("BlueprintCallable method SetActorHiddenInGame in class ACoverageUFunctionNativeCollisionActor already specified in superclass AActor.")
			},
			{
				TEXT("BlueprintOverride const mismatch"),
				TEXT("ASCoverageUFunction_InvalidOverrideConstMismatch"),
				TEXT("ASCoverageUFunctionInvalidOverrideConstMismatch.as"),
				ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageUFunctionConstMismatchBaseActor : AActor
				{
					UFUNCTION(BlueprintPure, BlueprintEvent)
					int ReadConstEvent(int Value) const
					{
						return Value;
					}
				}

				UCLASS()
				class ACoverageUFunctionConstMismatchChildActor : ACoverageUFunctionConstMismatchBaseActor
				{
					UFUNCTION(BlueprintOverride)
					int ReadConstEvent(int Value)
					{
						return Value + 1;
					}
				}
				)AS"),
				TEXT("BlueprintOverride method ReadConstEvent in class ACoverageUFunctionConstMismatchChildActor does not match signature of event declared in superclass ACoverageUFunctionConstMismatchBaseActor.")
			},
			{
				TEXT("optional UFUNCTION parameter unsupported"),
				TEXT("ASCoverageUFunction_InvalidOptionalParameter"),
				TEXT("ASCoverageUFunctionInvalidOptionalParameter.as"),
				ASTEST_AS(R"AS(
				USTRUCT(BlueprintType)
				struct FUFunctionOptionalParameterPayload
				{
					UPROPERTY()
					int Value = 0;
				}

				UCLASS()
				class ACoverageUFunctionOptionalParameterActor : AActor
				{
					UFUNCTION()
					void AcceptOptional(TOptional<FUFunctionOptionalParameterPayload> Value)
					{
					}
				}
				)AS"),
				TEXT("Unknown or invalid parameter type for parameter Value")
			},
		};

		for (const FInvalidUFunctionCase& InvalidCase : Cases)
		{
			FAngelscriptCompileTraceSummary Summary;
			const bool bCompiled = CompileModuleWithSummary(
				&Engine,
				ECompileType::FullReload,
				InvalidCase.ModuleName,
				InvalidCase.Filename,
				InvalidCase.ScriptSource,
				/*bUsePreprocessor=*/ true,
				Summary,
				/*bSuppressCompileErrorLogs=*/ true);

			const FString CompileMessage = FString::Printf(TEXT("%s should fail to compile"), InvalidCase.Label);
			ASSERT_THAT(IsFalse(bCompiled,
				*CompileMessage));
			const FString ResultMessage = FString::Printf(TEXT("%s should surface a compile error result"), InvalidCase.Label);
			ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult,
				*ResultMessage));
			const FString DiagnosticMessage = FString::Printf(TEXT("%s should report expected diagnostic: %s"), InvalidCase.Label, InvalidCase.ExpectedDiagnostic);
			ASSERT_THAT(IsTrue(CompileSummaryHasErrorContaining(Summary, InvalidCase.ExpectedDiagnostic),
				*DiagnosticMessage));

			Engine.DiscardModule(*InvalidCase.ModuleName.ToString());
			Engine.ResetDiagnostics();
			Engine.LastEmittedDiagnostics.Empty();
		}
	}

	TEST_METHOD(MixedParameterReflectionAndRuntimeCall)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUFunction_MixedParameterRuntimeCall"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUFunctionMixedParameterRuntimeCall.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FUFunctionPayload
			{
				UPROPERTY()
				int Count = 0;

				UPROPERTY()
				FString Label;
			}

			UCLASS()
			class ACoverageUFunctionMixedParameterActor : AActor
			{
				UPROPERTY()
				int LastScore = 0;

				UPROPERTY()
				FString LastLabel;

				UPROPERTY()
				FName LastName = NAME_None;

				UPROPERTY()
				FText LastText;

				UPROPERTY()
				FVector LastVector = FVector::ZeroVector;

				UPROPERTY()
				UObject LastObject;

				UPROPERTY()
				FUFunctionPayload LastPayload;

				UFUNCTION(BlueprintCallable, Category="Coverage|MixedParameters")
				int AcceptMixedParameters(UObject ObjectValue, FName NameValue, const FText&in TextValue, FVector VectorValue, const FUFunctionPayload&in Payload, int Score = 5)
				{
					LastObject = ObjectValue;
					LastName = NameValue;
					LastText = TextValue;
					LastVector = VectorValue;
					LastPayload = Payload;
					LastScore = Score + Payload.Count + int(VectorValue.X);
					LastLabel = Payload.Label + ":" + TextValue.ToString();
					return LastScore;
				}

				UFUNCTION(BlueprintCallable, Category="Coverage|MixedParameters")
				void FillMixedOut(FName&out OutName, FText&out OutText, FVector&out OutVector, FUFunctionPayload&out OutPayload)
				{
					OutName = n"GeneratedName";
					OutText = FText::FromString("GeneratedText");
					OutVector = FVector(9.0, 8.0, 7.0);
					OutPayload.Count = 44;
					OutPayload.Label = "GeneratedPayload";
				}
			}
			)AS"),
			TEXT("ACoverageUFunctionMixedParameterActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Mixed UFUNCTION parameter actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UFunction* AcceptMixedParameters = FindFunctionForTest(ScriptClass, TEXT("AcceptMixedParameters"));
		UFunction* FillMixedOut = FindFunctionForTest(ScriptClass, TEXT("FillMixedOut"));
		ASSERT_THAT(IsNotNull(AcceptMixedParameters, TEXT("AcceptMixedParameters should be generated")));
		ASSERT_THAT(IsNotNull(FillMixedOut, TEXT("FillMixedOut should be generated")));
		if (AcceptMixedParameters == nullptr || FillMixedOut == nullptr)
		{
			return;
		}

		FObjectProperty* ObjectParam = CastField<FObjectProperty>(FindParameterForTest(AcceptMixedParameters, TEXT("ObjectValue")));
		FNameProperty* NameParam = CastField<FNameProperty>(FindParameterForTest(AcceptMixedParameters, TEXT("NameValue")));
		FTextProperty* TextParam = CastField<FTextProperty>(FindParameterForTest(AcceptMixedParameters, TEXT("TextValue")));
		FStructProperty* VectorParam = CastField<FStructProperty>(FindParameterForTest(AcceptMixedParameters, TEXT("VectorValue")));
		FStructProperty* PayloadParam = CastField<FStructProperty>(FindParameterForTest(AcceptMixedParameters, TEXT("Payload")));
		FIntProperty* ScoreParam = CastField<FIntProperty>(FindParameterForTest(AcceptMixedParameters, TEXT("Score")));
		FIntProperty* ReturnValue = CastField<FIntProperty>(FindParameterForTest(AcceptMixedParameters, TEXT("ReturnValue")));
		ASSERT_THAT(IsNotNull(ObjectParam, TEXT("UObject parameter should reflect as FObjectProperty")));
		ASSERT_THAT(IsNotNull(NameParam, TEXT("FName parameter should reflect as FNameProperty")));
		ASSERT_THAT(IsNotNull(TextParam, TEXT("FText parameter should reflect as FTextProperty")));
		ASSERT_THAT(IsNotNull(VectorParam, TEXT("FVector parameter should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(PayloadParam, TEXT("AS USTRUCT parameter should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(ScoreParam, TEXT("default int parameter should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(ReturnValue, TEXT("int return should reflect as FIntProperty")));
		if (ObjectParam == nullptr || NameParam == nullptr || TextParam == nullptr || VectorParam == nullptr
			|| PayloadParam == nullptr || ScoreParam == nullptr || ReturnValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(UObject::StaticClass(), ObjectParam->PropertyClass,
			TEXT("UObject parameter should preserve the UObject class")));
		ASSERT_THAT(AreEqual(TBaseStructure<FVector>::Get(), VectorParam->Struct,
			TEXT("FVector parameter should preserve the native struct type")));
		ASSERT_THAT(IsTrue(TextParam->HasAnyPropertyFlags(CPF_ConstParm),
			TEXT("const FText&in should carry CPF_ConstParm")));
		ASSERT_THAT(IsTrue(TextParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("const FText&in should carry CPF_OutParm reference metadata")));
		ASSERT_THAT(IsTrue(PayloadParam->HasAnyPropertyFlags(CPF_ConstParm),
			TEXT("const AS USTRUCT&in should carry CPF_ConstParm")));
		ASSERT_THAT(IsTrue(PayloadParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("const AS USTRUCT&in should carry CPF_OutParm reference metadata")));
		ASSERT_THAT(IsTrue(ReturnValue->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("mixed parameter function return should carry CPF_ReturnParm")));

		FIntProperty* PayloadCountProperty = FindFProperty<FIntProperty>(PayloadParam->Struct, TEXT("Count"));
		FStrProperty* PayloadLabelProperty = FindFProperty<FStrProperty>(PayloadParam->Struct, TEXT("Label"));
		ASSERT_THAT(IsNotNull(PayloadCountProperty, TEXT("FUFunctionPayload.Count should reflect")));
		ASSERT_THAT(IsNotNull(PayloadLabelProperty, TEXT("FUFunctionPayload.Label should reflect")));
		if (PayloadCountProperty == nullptr || PayloadLabelProperty == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Mixed UFUNCTION parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker AcceptInvoker(*TestRunner, Actor, TEXT("AcceptMixedParameters"));
		ASSERT_THAT(IsTrue(AcceptInvoker.IsValid(), TEXT("AcceptMixedParameters should be invokable through reflection")));
		if (!AcceptInvoker.IsValid())
		{
			return;
		}
		AcceptInvoker.AddParam<UObject*>(Actor);
		AcceptInvoker.AddParam<FName>(FName(TEXT("InputName")));
		AcceptInvoker.AddParam<FText>(FText::FromString(TEXT("InputText")));
		AcceptInvoker.AddParam<FVector>(FVector(3.0, 4.0, 5.0));

		FProperty* PayloadSlotProperty = nullptr;
		void* PayloadSlot = nullptr;
		ASSERT_THAT(IsTrue(AcceptInvoker.AddParamSlot(PayloadSlotProperty, PayloadSlot),
			TEXT("AcceptMixedParameters should expose the AS USTRUCT parameter slot")));
		FStructProperty* PayloadSlotStructProperty = CastField<FStructProperty>(PayloadSlotProperty);
		ASSERT_THAT(IsNotNull(PayloadSlotStructProperty,
			TEXT("Payload parameter slot should be an FStructProperty")));
		if (PayloadSlot == nullptr || PayloadSlotStructProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(PayloadParam->Struct, PayloadSlotStructProperty->Struct,
			TEXT("Payload parameter slot should use the generated AS struct type")));
		if (PayloadSlotStructProperty->Struct != PayloadParam->Struct)
		{
			return;
		}
		PayloadCountProperty->SetPropertyValue_InContainer(PayloadSlot, 11);
		PayloadLabelProperty->SetPropertyValue_InContainer(PayloadSlot, FString(TEXT("PayloadLabel")));
		AcceptInvoker.AddParam<int32>(7);
		ASSERT_THAT(AreEqual(21, AcceptInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("mixed UFUNCTION parameter invocation should return computed script result")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastScore"), 21,
			TEXT("mixed UFUNCTION parameters should update int state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastLabel"), FString(TEXT("PayloadLabel:InputText")),
			TEXT("mixed UFUNCTION FText and USTRUCT fields should compose script state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("LastName"), FName(TEXT("InputName")),
			TEXT("mixed UFUNCTION FName parameter should round-trip"))));
		FVector LastVector = FVector::ZeroVector;
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("LastVector"), LastVector),
			TEXT("LastVector should be readable after mixed parameter call")));
		ASSERT_THAT(IsTrue(LastVector.Equals(FVector(3.0, 4.0, 5.0), 0.001),
			TEXT("mixed UFUNCTION FVector parameter should round-trip")));
		UObject* LastObject = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("LastObject"), LastObject),
			TEXT("LastObject should be readable after mixed parameter call")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), LastObject,
			TEXT("mixed UFUNCTION UObject parameter should round-trip")));

		FFunctionInvoker OutInvoker(*TestRunner, Actor, TEXT("FillMixedOut"));
		ASSERT_THAT(IsTrue(OutInvoker.IsValid(), TEXT("FillMixedOut should be invokable through reflection")));
		if (!OutInvoker.IsValid())
		{
			return;
		}
		FProperty* OutNameProperty = nullptr;
		void* OutNameSlot = nullptr;
		FProperty* OutTextProperty = nullptr;
		void* OutTextSlot = nullptr;
		FProperty* OutVectorProperty = nullptr;
		void* OutVectorSlot = nullptr;
		FProperty* OutPayloadProperty = nullptr;
		void* OutPayloadSlot = nullptr;
		ASSERT_THAT(IsTrue(OutInvoker.AddParamSlot(OutNameProperty, OutNameSlot), TEXT("FillMixedOut should expose FName out slot")));
		ASSERT_THAT(IsTrue(OutInvoker.AddParamSlot(OutTextProperty, OutTextSlot), TEXT("FillMixedOut should expose FText out slot")));
		ASSERT_THAT(IsTrue(OutInvoker.AddParamSlot(OutVectorProperty, OutVectorSlot), TEXT("FillMixedOut should expose FVector out slot")));
		ASSERT_THAT(IsTrue(OutInvoker.AddParamSlot(OutPayloadProperty, OutPayloadSlot), TEXT("FillMixedOut should expose payload out slot")));
		FNameProperty* OutNameParam = CastField<FNameProperty>(OutNameProperty);
		FTextProperty* OutTextParam = CastField<FTextProperty>(OutTextProperty);
		FStructProperty* OutVectorParam = CastField<FStructProperty>(OutVectorProperty);
		FStructProperty* OutPayloadParam = CastField<FStructProperty>(OutPayloadProperty);
		ASSERT_THAT(IsNotNull(OutNameParam, TEXT("FName out parameter should reflect as FNameProperty")));
		ASSERT_THAT(IsNotNull(OutTextParam, TEXT("FText out parameter should reflect as FTextProperty")));
		ASSERT_THAT(IsNotNull(OutVectorParam, TEXT("FVector out parameter should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(OutPayloadParam, TEXT("AS USTRUCT out parameter should reflect as FStructProperty")));
		if (OutNameSlot == nullptr || OutTextSlot == nullptr || OutVectorSlot == nullptr || OutPayloadSlot == nullptr
			|| OutNameParam == nullptr || OutTextParam == nullptr || OutVectorParam == nullptr || OutPayloadParam == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(OutNameParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("FName &out parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsTrue(OutTextParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("FText &out parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsTrue(OutVectorParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("FVector &out parameter should carry CPF_OutParm")));
		ASSERT_THAT(IsTrue(OutPayloadParam->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("AS USTRUCT &out parameter should carry CPF_OutParm")));
		ASSERT_THAT(AreEqual(PayloadParam->Struct, OutPayloadParam->Struct,
			TEXT("AS USTRUCT out parameter should use the same generated struct type")));
		if (OutPayloadParam->Struct != PayloadParam->Struct)
		{
			return;
		}
		ASSERT_THAT(IsTrue(OutInvoker.Call(), TEXT("FillMixedOut should write all reflected out parameters")));

		ASSERT_THAT(AreEqual(FName(TEXT("GeneratedName")), OutNameParam->GetPropertyValue(OutNameSlot),
			TEXT("FName &out value should be written into caller buffer")));
		ASSERT_THAT(AreEqual(FString(TEXT("GeneratedText")), OutTextParam->GetPropertyValue(OutTextSlot).ToString(),
			TEXT("FText &out value should be written into caller buffer")));
		const FVector& OutVectorValue = *static_cast<const FVector*>(OutVectorSlot);
		ASSERT_THAT(IsTrue(OutVectorValue.Equals(FVector(9.0, 8.0, 7.0), 0.001),
			TEXT("FVector &out value should be written into caller buffer")));
		ASSERT_THAT(AreEqual(44, PayloadCountProperty->GetPropertyValue_InContainer(OutPayloadSlot),
			TEXT("AS USTRUCT &out int field should be written into caller buffer")));
		ASSERT_THAT(AreEqual(FString(TEXT("GeneratedPayload")), PayloadLabelProperty->GetPropertyValue_InContainer(OutPayloadSlot),
			TEXT("AS USTRUCT &out string field should be written into caller buffer")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
