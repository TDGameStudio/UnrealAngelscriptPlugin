#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadReflectionMetadataTests,
	"Angelscript.TestModule.HotReload.ReflectionMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName FunctionMetadataModuleName = FName(TEXT("HotReloadReflectionFunctionMetadata"));
	inline static const FString FunctionMetadataFilename = FString(TEXT("HotReloadReflectionFunctionMetadata.as"));
	inline static const FName FunctionMetadataClassName = FName(TEXT("UHotReloadReflectionFunctionCarrier"));

	inline static const FName ClassAndEnumMetadataModuleName = FName(TEXT("HotReloadReflectionClassEnumMetadata"));
	inline static const FString ClassAndEnumMetadataFilename = FString(TEXT("HotReloadReflectionClassEnumMetadata.as"));
	inline static const FName ClassMetadataClassName = FName(TEXT("AHotReloadReflectionMetadataActor"));
	inline static const FString EnumMetadataName = FString(TEXT("EHotReloadReflectionMetadataState"));

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static UEnum* FindScriptEnum(FAngelscriptEngine& Engine, const FString& EnumName)
	{
		const TSharedPtr<FAngelscriptEnumDesc> EnumDesc = Engine.GetEnum(EnumName);
		return EnumDesc.IsValid() ? EnumDesc->Enum : nullptr;
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

	TEST_METHOD(FunctionMetadataAndDefaultsUpdateAfterFullReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FunctionMetadataModuleName.ToString());
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadReflectionFunctionCarrier : UObject
			{
				UFUNCTION(meta=(DisplayName="Alpha Function", ToolTip="Alpha tooltip"))
				int ComputeValue(int Value = 3)
				{
					return Value;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, FunctionMetadataModuleName, FunctionMetadataFilename, ReloadV1Source),
			TEXT("Initial reflection metadata function module should compile")));

		UClass* ClassBeforeReload = FindGeneratedClass(&Engine, FunctionMetadataClassName);
		ASSERT_THAT(IsNotNull(ClassBeforeReload, TEXT("Function metadata carrier should exist before reload")));

		UFunction* FunctionBeforeReload = FindGeneratedFunction(ClassBeforeReload, TEXT("ComputeValue"));
		ASSERT_THAT(IsNotNull(FunctionBeforeReload, TEXT("ComputeValue should exist before metadata reload")));
		ASSERT_THAT(AreEqual(FString(TEXT("Alpha Function")), FunctionBeforeReload->GetMetaData(TEXT("DisplayName")), TEXT("Initial function DisplayName metadata should be reflected")));
		ASSERT_THAT(AreEqual(FString(TEXT("Alpha tooltip")), FunctionBeforeReload->GetMetaData(TEXT("ToolTip")), TEXT("Initial function ToolTip metadata should be reflected")));
		ASSERT_THAT(AreEqual(FString(TEXT("3")), FunctionBeforeReload->GetMetaData(TEXT("CPP_Default_Value")), TEXT("Initial default argument metadata should be reflected")));

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			UCLASS()
			class UHotReloadReflectionFunctionCarrier : UObject
			{
				UFUNCTION(BlueprintCallable, meta=(DisplayName="Beta Function", ToolTip="Beta tooltip"))
				int ComputeValue(int Value = 7)
				{
					return Value + 1;
				}
			}
			)AS");

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, FunctionMetadataModuleName, FunctionMetadataFilename, ReloadV2Source, ReloadResult),
			TEXT("Function metadata full reload should compile")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Function metadata full reload should be handled")));

		UClass* ClassAfterReload = FindGeneratedClass(&Engine, FunctionMetadataClassName);
		ASSERT_THAT(IsNotNull(ClassAfterReload, TEXT("Function metadata carrier should exist after reload")));
		ASSERT_THAT(AreNotEqual(ClassBeforeReload, ClassAfterReload, TEXT("Full reload should replace the function metadata carrier class")));

		UFunction* FunctionAfterReload = FindGeneratedFunction(ClassAfterReload, TEXT("ComputeValue"));
		ASSERT_THAT(IsNotNull(FunctionAfterReload, TEXT("ComputeValue should exist after metadata reload")));
		ASSERT_THAT(AreNotEqual(FunctionBeforeReload, FunctionAfterReload, TEXT("Full reload should replace the reflected UFunction")));

		ASSERT_THAT(AreEqual(FString(TEXT("Alpha Function")), FunctionBeforeReload->GetMetaData(TEXT("DisplayName")), TEXT("Old function should keep its original DisplayName metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("3")), FunctionBeforeReload->GetMetaData(TEXT("CPP_Default_Value")), TEXT("Old function should keep its original default metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Beta Function")), FunctionAfterReload->GetMetaData(TEXT("DisplayName")), TEXT("Reloaded function should expose the new DisplayName metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Beta tooltip")), FunctionAfterReload->GetMetaData(TEXT("ToolTip")), TEXT("Reloaded function should expose the new ToolTip metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("7")), FunctionAfterReload->GetMetaData(TEXT("CPP_Default_Value")), TEXT("Reloaded function should expose the new default argument metadata")));
		ASSERT_THAT(IsTrue(FunctionAfterReload->HasAnyFunctionFlags(FUNC_BlueprintCallable), TEXT("Reloaded function should reflect the BlueprintCallable specifier")));
	}

	TEST_METHOD(ClassAndEnumMetadataUpdateAfterFullReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ClassAndEnumMetadataModuleName.ToString());
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			UENUM(meta=(ToolTip="Alpha enum tooltip"))
			enum class EHotReloadReflectionMetadataState : uint8
			{
				Alpha UMETA(DisplayName="Alpha State", ToolTip="Alpha state tooltip"),
				Beta
			}

			UCLASS(meta=(DisplayName="Alpha Actor", ToolTip="Alpha class tooltip"))
			class AHotReloadReflectionMetadataActor : AActor
			{
				UPROPERTY(meta=(DisplayName="Alpha Value", ToolTip="Alpha value tooltip"))
				int Value = 1;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, ClassAndEnumMetadataModuleName, ClassAndEnumMetadataFilename, ReloadV1Source),
			TEXT("Initial class/enum reflection metadata module should compile")));

		UClass* ClassBeforeReload = FindGeneratedClass(&Engine, ClassMetadataClassName);
		UEnum* EnumBeforeReload = FindScriptEnum(Engine, EnumMetadataName);
		ASSERT_THAT(IsNotNull(ClassBeforeReload, TEXT("Class metadata carrier should exist before reload")));
		ASSERT_THAT(IsNotNull(EnumBeforeReload, TEXT("Enum metadata object should exist before reload")));

		FProperty* PropertyBeforeReload = FindFProperty<FProperty>(ClassBeforeReload, TEXT("Value"));
		ASSERT_THAT(IsNotNull(PropertyBeforeReload, TEXT("Value property should exist before reload")));
		ASSERT_THAT(AreEqual(FString(TEXT("Alpha Actor")), ClassBeforeReload->GetMetaData(TEXT("DisplayName")), TEXT("Initial class DisplayName metadata should be reflected")));
		ASSERT_THAT(AreEqual(FString(TEXT("Alpha Value")), PropertyBeforeReload->GetMetaData(TEXT("DisplayName")), TEXT("Initial property DisplayName metadata should be reflected")));
		ASSERT_THAT(AreEqual(FString(TEXT("Alpha State")), EnumBeforeReload->GetMetaData(TEXT("DisplayName"), 0), TEXT("Initial enum value DisplayName metadata should be reflected")));

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			UENUM(meta=(ToolTip="Beta enum tooltip"))
			enum class EHotReloadReflectionMetadataState : uint8
			{
				Alpha UMETA(DisplayName="Beta State", ToolTip="Beta state tooltip"),
				Beta
			}

			UCLASS(Abstract, meta=(DisplayName="Beta Actor", ToolTip="Beta class tooltip"))
			class AHotReloadReflectionMetadataActor : AActor
			{
				UPROPERTY(EditAnywhere, meta=(DisplayName="Beta Value", ToolTip="Beta value tooltip"))
				int Value = 2;
			}
			)AS");

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, ClassAndEnumMetadataModuleName, ClassAndEnumMetadataFilename, ReloadV2Source, ReloadResult),
			TEXT("Class/enum metadata full reload should compile")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Class/enum metadata full reload should be handled")));

		UClass* ClassAfterReload = FindGeneratedClass(&Engine, ClassMetadataClassName);
		UEnum* EnumAfterReload = FindScriptEnum(Engine, EnumMetadataName);
		ASSERT_THAT(IsNotNull(ClassAfterReload, TEXT("Class metadata carrier should exist after reload")));
		ASSERT_THAT(IsNotNull(EnumAfterReload, TEXT("Enum metadata object should exist after reload")));
		ASSERT_THAT(AreNotEqual(ClassBeforeReload, ClassAfterReload, TEXT("Full reload should replace the metadata class")));

		FProperty* PropertyAfterReload = FindFProperty<FProperty>(ClassAfterReload, TEXT("Value"));
		ASSERT_THAT(IsNotNull(PropertyAfterReload, TEXT("Value property should exist after reload")));

		ASSERT_THAT(AreEqual(FString(TEXT("Alpha Actor")), ClassBeforeReload->GetMetaData(TEXT("DisplayName")), TEXT("Old class should keep original DisplayName metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Alpha Value")), PropertyBeforeReload->GetMetaData(TEXT("DisplayName")), TEXT("Old property should keep original DisplayName metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Beta Actor")), ClassAfterReload->GetMetaData(TEXT("DisplayName")), TEXT("Reloaded class should expose new DisplayName metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Beta class tooltip")), ClassAfterReload->GetMetaData(TEXT("ToolTip")), TEXT("Reloaded class should expose new ToolTip metadata")));
		ASSERT_THAT(IsTrue(ClassAfterReload->HasAnyClassFlags(CLASS_Abstract), TEXT("Reloaded class should reflect the Abstract specifier")));
		ASSERT_THAT(AreEqual(FString(TEXT("Beta Value")), PropertyAfterReload->GetMetaData(TEXT("DisplayName")), TEXT("Reloaded property should expose new DisplayName metadata")));
		ASSERT_THAT(IsTrue(PropertyAfterReload->HasAnyPropertyFlags(CPF_Edit), TEXT("Reloaded property should reflect EditAnywhere")));
		ASSERT_THAT(AreEqual(FString(TEXT("Beta enum tooltip")), EnumAfterReload->GetMetaData(TEXT("ToolTip")), TEXT("Reloaded enum should expose new ToolTip metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Beta State")), EnumAfterReload->GetMetaData(TEXT("DisplayName"), 0), TEXT("Reloaded enum value should expose new DisplayName metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Beta State")), EnumAfterReload->GetDisplayNameTextByIndex(0).ToString(), TEXT("Reloaded enum display text should use the new DisplayName metadata")));
	}
};

#endif
