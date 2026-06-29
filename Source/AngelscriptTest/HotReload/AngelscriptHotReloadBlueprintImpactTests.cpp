#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "BlueprintImpact/AngelscriptBlueprintImpactScanner.h"
#include "Functional/Blueprint/AngelscriptBlueprintTestHelpers.h"

#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadBlueprintImpactTests,
	"Angelscript.TestModule.HotReload.BlueprintImpact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName StructImpactModuleName = FName(TEXT("HotReloadBlueprintImpactStruct"));
	inline static const FString StructImpactFilename = FString(TEXT("HotReloadBlueprintImpactStruct.as"));
	inline static const FString StructImpactStructName = FString(TEXT("FHotReloadBlueprintImpactPayload"));

	inline static const FName EnumImpactModuleName = FName(TEXT("HotReloadBlueprintImpactEnum"));
	inline static const FString EnumImpactFilename = FString(TEXT("HotReloadBlueprintImpactEnum.as"));
	inline static const FString EnumImpactEnumName = FString(TEXT("EHotReloadBlueprintImpactState"));

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static UScriptStruct* FindScriptStruct(FAngelscriptEngine& Engine, const FString& StructName)
	{
		const TSharedPtr<FAngelscriptClassDesc> StructDesc = Engine.GetClass(StructName);
		return StructDesc.IsValid() ? Cast<UScriptStruct>(StructDesc->Struct) : nullptr;
	}

	static UEnum* FindScriptEnum(FAngelscriptEngine& Engine, const FString& EnumName)
	{
		const TSharedPtr<FAngelscriptEnumDesc> EnumDesc = Engine.GetEnum(EnumName);
		return EnumDesc.IsValid() ? EnumDesc->Enum : nullptr;
	}

	static UK2Node_CustomEvent* AddCustomEventUserPin(UBlueprint& Blueprint, const FEdGraphPinType& PinType)
	{
		UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(&Blueprint);
		if (EventGraph == nullptr)
		{
			return nullptr;
		}

		UK2Node_CustomEvent* CustomEventNode = NewObject<UK2Node_CustomEvent>(EventGraph);
		EventGraph->AddNode(CustomEventNode, false, false);
		CustomEventNode->CustomFunctionName = TEXT("HotReloadBlueprintImpactEvent");
		CustomEventNode->CreateNewGuid();
		CustomEventNode->PostPlacedNewNode();
		CustomEventNode->AllocateDefaultPins();
		CustomEventNode->CreateUserDefinedPin(TEXT("Payload"), PinType, EGPD_Output);
		return CustomEventNode;
	}

	static FEdGraphPinType MakeStructPinType(UScriptStruct* Struct)
	{
		FEdGraphPinType PinType;
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = Struct;
		return PinType;
	}

	static FEdGraphPinType MakeEnumPinType(UEnum* Enum)
	{
		FEdGraphPinType PinType;
		PinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
		PinType.PinSubCategoryObject = Enum;
		return PinType;
	}

	static void AddBlueprintVariable(UBlueprint& Blueprint, FName VariableName, const FEdGraphPinType& PinType)
	{
		FBPVariableDescription Variable;
		Variable.VarName = VariableName;
		Variable.VarType = PinType;
		Blueprint.NewVariables.Add(Variable);
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

	TEST_METHOD(StructLayoutReloadMarksBlueprintVariablesAndPinsImpacted)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		AngelscriptBlueprintTestUtils::FScopedTransientBlueprint Blueprint;
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*StructImpactModuleName.ToString());
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			USTRUCT()
			struct FHotReloadBlueprintImpactPayload
			{
				UPROPERTY()
				int Value = 1;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, StructImpactModuleName, StructImpactFilename, ReloadV1Source),
			TEXT("Initial BlueprintImpact struct module should compile")));

		UScriptStruct* StructBeforeReload = FindScriptStruct(Engine, StructImpactStructName);
		ASSERT_THAT(IsNotNull(StructBeforeReload, TEXT("BlueprintImpact struct should exist before reload")));

		ASSERT_THAT(IsTrue(Blueprint.CreateAndCompile(*TestRunner, UObject::StaticClass(), TEXT("HotReloadStructImpact"), TEXT("AngelscriptHotReloadBlueprintImpactTests"))));
		ASSERT_THAT(IsNotNull(Blueprint.Blueprint, TEXT("BlueprintImpact struct test should create a transient Blueprint")));

		const FEdGraphPinType StructPinType = MakeStructPinType(StructBeforeReload);
		AddBlueprintVariable(*Blueprint.Blueprint, TEXT("PayloadVariable"), StructPinType);
		ASSERT_THAT(IsNotNull(AddCustomEventUserPin(*Blueprint.Blueprint, StructPinType), TEXT("BlueprintImpact struct test should add a user pin using the script struct")));

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			USTRUCT()
			struct FHotReloadBlueprintImpactPayload
			{
				UPROPERTY()
				int Value = 1;

				UPROPERTY()
				int Bonus = 2;
			}
			)AS");

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, StructImpactModuleName, StructImpactFilename, ReloadV2Source, ReloadResult),
			TEXT("BlueprintImpact struct full reload should compile")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("BlueprintImpact struct full reload should be handled")));

		UScriptStruct* StructAfterReload = FindScriptStruct(Engine, StructImpactStructName);
		ASSERT_THAT(IsNotNull(StructAfterReload, TEXT("BlueprintImpact struct should exist after reload")));
		ASSERT_THAT(AreNotEqual(StructBeforeReload, StructAfterReload, TEXT("BlueprintImpact struct full reload should replace the struct object")));

		AngelscriptEditor::BlueprintImpact::FBlueprintImpactSymbols OldSymbols;
		OldSymbols.Structs.Add(StructBeforeReload);
		OldSymbols.ReplacementObjects.Add(StructBeforeReload, StructAfterReload);

		TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> Reasons;
		ASSERT_THAT(IsTrue(
			AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*Blueprint.Blueprint, OldSymbols, Reasons),
			TEXT("Blueprint using the old script struct should be marked impacted")));
		ASSERT_THAT(IsTrue(
			Reasons.Contains(AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason::PinType),
			TEXT("Struct BlueprintImpact should report pin-type impact")));
		ASSERT_THAT(IsTrue(
			Reasons.Contains(AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason::VariableType),
			TEXT("Struct BlueprintImpact should report variable-type impact")));

		FEdGraphPinType& VariablePinType = Blueprint.Blueprint->NewVariables[0].VarType;
		ASSERT_THAT(AreEqual(StructBeforeReload, Cast<UScriptStruct>(VariablePinType.PinSubCategoryObject.Get()), TEXT("Blueprint variable should still reference the old struct before hot reload retargeting")));
		VariablePinType.PinSubCategoryObject = StructAfterReload;
		ASSERT_THAT(AreEqual(StructAfterReload, Cast<UScriptStruct>(VariablePinType.PinSubCategoryObject.Get()), TEXT("Blueprint variable can retarget to the reloaded script struct")));
	}

	TEST_METHOD(EnumReloadMarksBlueprintVariablesAndPinsImpacted)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		AngelscriptBlueprintTestUtils::FScopedTransientBlueprint Blueprint;
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*EnumImpactModuleName.ToString());
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum class EHotReloadBlueprintImpactState : uint8
			{
				Alpha,
				Beta
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, EnumImpactModuleName, EnumImpactFilename, ReloadV1Source),
			TEXT("Initial BlueprintImpact enum module should compile")));

		UEnum* EnumBeforeReload = FindScriptEnum(Engine, EnumImpactEnumName);
		ASSERT_THAT(IsNotNull(EnumBeforeReload, TEXT("BlueprintImpact enum should exist before reload")));

		ASSERT_THAT(IsTrue(Blueprint.CreateAndCompile(*TestRunner, UObject::StaticClass(), TEXT("HotReloadEnumImpact"), TEXT("AngelscriptHotReloadBlueprintImpactTests"))));
		ASSERT_THAT(IsNotNull(Blueprint.Blueprint, TEXT("BlueprintImpact enum test should create a transient Blueprint")));

		const FEdGraphPinType EnumPinType = MakeEnumPinType(EnumBeforeReload);
		AddBlueprintVariable(*Blueprint.Blueprint, TEXT("StateVariable"), EnumPinType);
		ASSERT_THAT(IsNotNull(AddCustomEventUserPin(*Blueprint.Blueprint, EnumPinType), TEXT("BlueprintImpact enum test should add a user pin using the script enum")));

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum class EHotReloadBlueprintImpactState : uint8
			{
				Alpha,
				Beta,
				Gamma
			}
			)AS");

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, EnumImpactModuleName, EnumImpactFilename, ReloadV2Source, ReloadResult),
			TEXT("BlueprintImpact enum full reload should compile")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("BlueprintImpact enum full reload should be handled")));

		UEnum* EnumAfterReload = FindScriptEnum(Engine, EnumImpactEnumName);
		ASSERT_THAT(IsNotNull(EnumAfterReload, TEXT("BlueprintImpact enum should exist after reload")));

		AngelscriptEditor::BlueprintImpact::FBlueprintImpactSymbols OldSymbols;
		OldSymbols.Enums.Add(EnumBeforeReload);
		OldSymbols.ReplacementObjects.Add(EnumBeforeReload, EnumAfterReload);

		TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> Reasons;
		ASSERT_THAT(IsTrue(
			AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*Blueprint.Blueprint, OldSymbols, Reasons),
			TEXT("Blueprint using the old script enum should be marked impacted")));
		ASSERT_THAT(IsTrue(
			Reasons.Contains(AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason::PinType),
			TEXT("Enum BlueprintImpact should report pin-type impact")));
		ASSERT_THAT(IsTrue(
			Reasons.Contains(AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason::VariableType),
			TEXT("Enum BlueprintImpact should report variable-type impact")));

		FEdGraphPinType& VariablePinType = Blueprint.Blueprint->NewVariables[0].VarType;
		ASSERT_THAT(AreEqual(EnumBeforeReload, Cast<UEnum>(VariablePinType.PinSubCategoryObject.Get()), TEXT("Blueprint variable should still reference the old enum before hot reload retargeting")));
		VariablePinType.PinSubCategoryObject = EnumAfterReload;
		ASSERT_THAT(AreEqual(EnumAfterReload, Cast<UEnum>(VariablePinType.PinSubCategoryObject.Get()), TEXT("Blueprint variable can retarget to the reloaded script enum")));
	}
};

#endif
