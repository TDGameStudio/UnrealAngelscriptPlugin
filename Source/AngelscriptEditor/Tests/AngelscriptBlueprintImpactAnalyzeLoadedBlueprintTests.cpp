#include "BlueprintImpact/AngelscriptBlueprintImpactScanner.h"

#include "CQTest.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MacroInstance.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "EdGraphSchema_K2.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactAnalyzeLoadedBlueprintTests_Private
{
	UBlueprint* CreateTransientBlueprintChild(FAutomationTestBase& Test, UClass* ParentClass, FStringView Suffix)
	{
		const FString PackagePath = FString::Printf(
			TEXT("/Temp/AngelscriptBlueprintImpactAnalyze_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));

		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint negative-path test should create a transient package"), BlueprintPackage))
		{
			return nullptr;
		}

		BlueprintPackage->SetFlags(RF_Transient);
		const FName BlueprintName(*FPackageName::GetLongPackageAssetName(PackagePath));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			BlueprintPackage,
			BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("AngelscriptBlueprintImpactAnalyzeLoadedBlueprintTests"));
		if (Blueprint != nullptr)
		{
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
		}
		return Blueprint;
	}

	void CleanupBlueprint(UBlueprint*& Blueprint)
	{
		if (Blueprint == nullptr)
		{
			return;
		}

		if (UClass* BlueprintClass = Blueprint->GeneratedClass)
		{
			BlueprintClass->MarkAsGarbage();
		}

		if (UPackage* BlueprintPackage = Blueprint->GetOutermost())
		{
			BlueprintPackage->MarkAsGarbage();
		}

		Blueprint->MarkAsGarbage();
		CollectGarbage(RF_NoFlags, true);
		Blueprint = nullptr;
	}

	void MarkBlueprintForCleanup(UBlueprint*& Blueprint)
	{
		if (!IsValid(Blueprint))
		{
			Blueprint = nullptr;
			return;
		}

		if (UClass* BlueprintClass = Blueprint->GeneratedClass; IsValid(BlueprintClass))
		{
			BlueprintClass->MarkAsGarbage();
		}

		if (UPackage* BlueprintPackage = Blueprint->GetOutermost(); IsValid(BlueprintPackage))
		{
			BlueprintPackage->MarkAsGarbage();
		}

		Blueprint->MarkAsGarbage();
		Blueprint = nullptr;
	}

	UK2Node_CustomEvent* AddCustomEventUserPin(UBlueprint& Blueprint, const FEdGraphPinType& PinType)
	{
		UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(&Blueprint);
		if (EventGraph == nullptr)
		{
			return nullptr;
		}

		UK2Node_CustomEvent* CustomEventNode = NewObject<UK2Node_CustomEvent>(EventGraph);
		EventGraph->AddNode(CustomEventNode, false, false);
		CustomEventNode->CustomFunctionName = TEXT("BlueprintImpactEditablePinEvent");
		CustomEventNode->CreateNewGuid();
		CustomEventNode->PostPlacedNewNode();
		CustomEventNode->AllocateDefaultPins();
		CustomEventNode->CreateUserDefinedPin(TEXT("ImpactVector"), PinType, EGPD_Output);
		return CustomEventNode;
	}

	UK2Node_MacroInstance* AddMacroInstanceWithResolvedWildcard(UBlueprint& Blueprint, const FEdGraphPinType& PinType)
	{
		UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(&Blueprint);
		if (EventGraph == nullptr)
		{
			return nullptr;
		}

		UK2Node_MacroInstance* MacroNode = NewObject<UK2Node_MacroInstance>(EventGraph);
		EventGraph->AddNode(MacroNode, false, false);
		MacroNode->CreateNewGuid();
		MacroNode->ResolvedWildcardType = PinType;
		return MacroNode;
	}

	UK2Node_CallFunction* AddCallFunctionNode(UBlueprint& Blueprint, UFunction* Function)
	{
		if (Function == nullptr)
		{
			return nullptr;
		}

		UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(&Blueprint);
		if (EventGraph == nullptr)
		{
			return nullptr;
		}

		UK2Node_CallFunction* CallFunctionNode = NewObject<UK2Node_CallFunction>(EventGraph);
		EventGraph->AddNode(CallFunctionNode, false, false);
		CallFunctionNode->CreateNewGuid();
		CallFunctionNode->PostPlacedNewNode();
		CallFunctionNode->SetFromFunction(Function);
		CallFunctionNode->AllocateDefaultPins();
		return CallFunctionNode;
	}
}

#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)
#define TestTrue(...) Test.TestTrue(__VA_ARGS__)

static bool RunEmptySymbolsReturnsFalseAndClearsReasons(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactAnalyzeLoadedBlueprintTests_Private;
	UBlueprint* Blueprint = CreateTransientBlueprintChild(Test, UObject::StaticClass(), TEXT("EmptySymbols"));
	ON_SCOPE_EXIT
	{
		CleanupBlueprint(Blueprint);
	};

	if (!TestNotNull(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint negative-path test should create a transient blueprint"), Blueprint))
	{
		return false;
	}

	TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> Reasons = {
		AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason::NodeDependency
	};

	const AngelscriptEditor::BlueprintImpact::FBlueprintImpactSymbols EmptySymbols;
	const bool bEmptySymbolsImpacted = AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*Blueprint, EmptySymbols, Reasons);
	if (!TestFalse(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should return false when the impact symbols are empty"), bEmptySymbolsImpacted))
	{
		return false;
	}
	if (!TestEqual(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should clear prefilled reasons before handling the empty-symbol early return"), Reasons.Num(), 0))
	{
		return false;
	}

	Reasons = { AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason::ReferencedAsset };

	AngelscriptEditor::BlueprintImpact::FBlueprintImpactSymbols UnrelatedSymbols;
	UnrelatedSymbols.Classes.Add(AActor::StaticClass());

	const bool bUnrelatedSymbolsImpacted = AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*Blueprint, UnrelatedSymbols, Reasons);
	if (!TestFalse(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should return false for symbols unrelated to the blueprint"), bUnrelatedSymbolsImpacted))
	{
		return false;
	}
	if (!TestEqual(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should clear prefilled reasons before evaluating unrelated symbols"), Reasons.Num(), 0))
	{
		return false;
	}

	return TestFalse(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should not retain stale reasons after unrelated symbols return false"), Reasons.Contains(AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason::ReferencedAsset));
}

static bool RunEditablePinsAndMacroWildcardReportPinType(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactAnalyzeLoadedBlueprintTests_Private;
	FEdGraphPinType VectorPinType;
	VectorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	VectorPinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();

	AngelscriptEditor::BlueprintImpact::FBlueprintImpactSymbols Symbols;
	Symbols.Structs.Add(TBaseStructure<FVector>::Get());

	UBlueprint* EditablePinBlueprint = CreateTransientBlueprintChild(Test, UObject::StaticClass(), TEXT("EditablePins"));
	UBlueprint* EditablePinControlBlueprint = CreateTransientBlueprintChild(Test, UObject::StaticClass(), TEXT("EditablePinsControl"));
	UBlueprint* MacroWildcardBlueprint = CreateTransientBlueprintChild(Test, UObject::StaticClass(), TEXT("MacroWildcard"));
	UBlueprint* MacroWildcardControlBlueprint = CreateTransientBlueprintChild(Test, UObject::StaticClass(), TEXT("MacroWildcardControl"));
	ON_SCOPE_EXIT
	{
		MarkBlueprintForCleanup(EditablePinBlueprint);
		MarkBlueprintForCleanup(EditablePinControlBlueprint);
		MarkBlueprintForCleanup(MacroWildcardBlueprint);
		MarkBlueprintForCleanup(MacroWildcardControlBlueprint);
		CollectGarbage(RF_NoFlags, true);
	};

	if (!TestNotNull(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint editable-pin test should create the editable-pin blueprint"), EditablePinBlueprint)
		|| !TestNotNull(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint editable-pin test should create the editable-pin control blueprint"), EditablePinControlBlueprint)
		|| !TestNotNull(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint macro-wildcard test should create the macro blueprint"), MacroWildcardBlueprint)
		|| !TestNotNull(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint macro-wildcard test should create the macro control blueprint"), MacroWildcardControlBlueprint))
	{
		return false;
	}

	if (!TestNotNull(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint editable-pin test should add a custom event with a user-defined pin"), AddCustomEventUserPin(*EditablePinBlueprint, VectorPinType))
		|| !TestNotNull(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint macro-wildcard test should add a macro node with a resolved wildcard type"), AddMacroInstanceWithResolvedWildcard(*MacroWildcardBlueprint, VectorPinType))
		|| !TestNotNull(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint macro-wildcard control should add a macro node even without a resolved wildcard type"), AddMacroInstanceWithResolvedWildcard(*MacroWildcardControlBlueprint, FEdGraphPinType())))
	{
		return false;
	}

	TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> EditablePinReasons;
	const bool bEditablePinsImpacted = AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*EditablePinBlueprint, Symbols, EditablePinReasons);
	if (!TestTrue(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should report editable user pins that use impacted struct types"), bEditablePinsImpacted))
	{
		return false;
	}
	if (!TestEqual(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should report exactly one reason for editable user pins"), EditablePinReasons.Num(), 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should classify editable user pins as pin-type impact"), EditablePinReasons[0], AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason::PinType))
	{
		return false;
	}

	TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> EditablePinControlReasons;
	if (!TestFalse(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should ignore custom events that do not add user-defined pins"), AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*EditablePinControlBlueprint, Symbols, EditablePinControlReasons)))
	{
		return false;
	}
	if (!TestEqual(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should keep editable-pin control reasons empty"), EditablePinControlReasons.Num(), 0))
	{
		return false;
	}

	TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> MacroWildcardReasons;
	const bool bMacroWildcardImpacted = AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*MacroWildcardBlueprint, Symbols, MacroWildcardReasons);
	if (!TestTrue(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should report macro wildcard pin types that use impacted struct types"), bMacroWildcardImpacted))
	{
		return false;
	}
	if (!TestEqual(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should report exactly one reason for macro wildcard pin types"), MacroWildcardReasons.Num(), 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should classify macro wildcard pin types as pin-type impact"), MacroWildcardReasons[0], AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason::PinType))
	{
		return false;
	}

	TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> MacroWildcardControlReasons;
	if (!TestFalse(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should ignore macro nodes whose resolved wildcard type stays empty"), AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*MacroWildcardControlBlueprint, Symbols, MacroWildcardControlReasons)))
	{
		return false;
	}
	return TestEqual(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should keep macro wildcard control reasons empty"), MacroWildcardControlReasons.Num(), 0);
}

static bool RunEnumPinsAndVariablesReportExactReasons(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactAnalyzeLoadedBlueprintTests_Private;
	UBlueprint* Blueprint = CreateTransientBlueprintChild(Test, UObject::StaticClass(), TEXT("EnumPinsAndVariables"));
	ON_SCOPE_EXIT
	{
		CleanupBlueprint(Blueprint);
	};

	if (!TestNotNull(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint enum-path test should create a transient blueprint"), Blueprint))
	{
		return false;
	}

	UEnum* ImpactedEnum = StaticEnum<EAutoReceiveInput::Type>();
	UEnum* UnrelatedEnum = StaticEnum<EAutoPossessAI>();
	if (!TestNotNull(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint enum-path test should expose the impacted enum"), ImpactedEnum)
		|| !TestNotNull(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint enum-path test should expose the unrelated enum"), UnrelatedEnum))
	{
		return false;
	}

	FBPVariableDescription Variable;
	Variable.VarName = TEXT("ImpactEnumVariable");
	Variable.VarType.PinCategory = UEdGraphSchema_K2::PC_Byte;
	Variable.VarType.PinSubCategoryObject = ImpactedEnum;
	Blueprint->NewVariables.Add(Variable);

	UK2Node_CallFunction* CallFunctionNode = AddCallFunctionNode(*Blueprint, UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("MakeVector")));
	if (!TestNotNull(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint enum-path test should add a function node"), CallFunctionNode))
	{
		return false;
	}

	UEdGraphPin* EnumPin = nullptr;
	for (UEdGraphPin* Pin : CallFunctionNode->Pins)
	{
		if (Pin != nullptr
			&& Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec
			&& Pin->PinName != UEdGraphSchema_K2::PN_Self)
		{
			EnumPin = Pin;
			break;
		}
	}

	if (!TestNotNull(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint enum-path test should find a data pin to mutate"), EnumPin))
	{
		return false;
	}

	EnumPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
	EnumPin->PinType.PinSubCategoryObject = ImpactedEnum;

	AngelscriptEditor::BlueprintImpact::FBlueprintImpactSymbols Symbols;
	Symbols.Enums.Add(ImpactedEnum);

	TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> Reasons;
	const bool bImpacted = AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*Blueprint, Symbols, Reasons);
	if (!TestTrue(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should detect enum-backed variables and pins"), bImpacted))
	{
		return false;
	}

	if (!TestEqual(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should report exactly two reasons for the enum-backed variable and pin"), Reasons.Num(), 2))
	{
		return false;
	}
	if (!TestEqual(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should report pin-type impact before variable-type impact for enum-backed symbols"), Reasons[0], AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason::PinType))
	{
		return false;
	}
	if (!TestEqual(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should report variable-type impact as the second and only remaining enum-backed reason"), Reasons[1], AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason::VariableType))
	{
		return false;
	}

	AngelscriptEditor::BlueprintImpact::FBlueprintImpactSymbols UnrelatedSymbols;
	UnrelatedSymbols.Enums.Add(UnrelatedEnum);

	TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason> UnrelatedReasons;
	if (!TestFalse(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should ignore unrelated enums"), AngelscriptEditor::BlueprintImpact::AnalyzeLoadedBlueprint(*Blueprint, UnrelatedSymbols, UnrelatedReasons)))
	{
		return false;
	}

	return TestEqual(TEXT("BlueprintImpact.AnalyzeLoadedBlueprint should keep unrelated enum reasons empty"), UnrelatedReasons.Num(), 0);
}

#undef TestTrue
#undef TestNotNull
#undef TestFalse
#undef TestEqual

TEST_CLASS_WITH_FLAGS(FAngelscriptBlueprintImpactAnalyzeLoadedBlueprintTests,
	"Angelscript.Editor.BlueprintImpact.AnalyzeLoadedBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EmptySymbolsReturnsFalseAndClearsReasons)
	{
		ASSERT_THAT(IsTrue(RunEmptySymbolsReturnsFalseAndClearsReasons(*TestRunner)));
	}

	TEST_METHOD(EditablePinsAndMacroWildcardReportPinType)
	{
		ASSERT_THAT(IsTrue(RunEditablePinsAndMacroWildcardReportPinType(*TestRunner)));
	}

	TEST_METHOD(EnumPinsAndVariablesReportExactReasons)
	{
		ASSERT_THAT(IsTrue(RunEnumPinsAndVariablesReportExactReasons(*TestRunner)));
	}
};

#endif
