#include "BlueprintImpact/AngelscriptBlueprintImpactScanner.h"

#include "CQTest.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactScannerCoreTests_Private
{
	FAngelscriptModuleDesc::FCodeSection MakeScannerCoreCodeSection(const FString& RelativeFilename)
	{
		FAngelscriptModuleDesc::FCodeSection Section;
		Section.RelativeFilename = RelativeFilename;
		Section.AbsoluteFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BlueprintImpact"), RelativeFilename);
		Section.Code = TEXT("// BlueprintImpact scanner core test");
		Section.CodeHash = GetTypeHash(RelativeFilename);
		return Section;
	}

	TSharedRef<FAngelscriptModuleDesc> MakeScannerCoreModule(
		const FString& ModuleName,
		std::initializer_list<FString> RelativeFilenames)
	{
		TSharedRef<FAngelscriptModuleDesc> Module = MakeShared<FAngelscriptModuleDesc>();
		Module->ModuleName = ModuleName;
		for (const FString& RelativeFilename : RelativeFilenames)
		{
			Module->Code.Add(MakeScannerCoreCodeSection(RelativeFilename));
		}
		return Module;
	}

	TSharedRef<FAngelscriptClassDesc> MakeClassDesc(const FString& ClassName, UClass* GeneratedClass)
	{
		TSharedRef<FAngelscriptClassDesc> ClassDesc = MakeShared<FAngelscriptClassDesc>();
		ClassDesc->ClassName = ClassName;
		ClassDesc->Class = GeneratedClass;
		return ClassDesc;
	}

	TSharedRef<FAngelscriptClassDesc> MakeStructDesc(const FString& StructName, UScriptStruct* GeneratedStruct)
	{
		TSharedRef<FAngelscriptClassDesc> StructDesc = MakeShared<FAngelscriptClassDesc>();
		StructDesc->ClassName = StructName;
		StructDesc->bIsStruct = true;
		StructDesc->Struct = GeneratedStruct;
		return StructDesc;
	}

	TSharedRef<FAngelscriptEnumDesc> MakeEnumDesc(const FString& EnumName, UEnum* GeneratedEnum)
	{
		TSharedRef<FAngelscriptEnumDesc> EnumDesc = MakeShared<FAngelscriptEnumDesc>();
		EnumDesc->EnumName = EnumName;
		EnumDesc->Enum = GeneratedEnum;
		return EnumDesc;
	}

	TSharedRef<FAngelscriptDelegateDesc> MakeDelegateDesc(const FString& DelegateName, UDelegateFunction* SignatureFunction)
	{
		TSharedRef<FAngelscriptDelegateDesc> DelegateDesc = MakeShared<FAngelscriptDelegateDesc>();
		DelegateDesc->DelegateName = DelegateName;
		DelegateDesc->Function = SignatureFunction;
		return DelegateDesc;
	}

	UDelegateFunction* FindActorDelegateSignature(FAutomationTestBase& Test, const FName PropertyName)
	{
		const FMulticastDelegateProperty* DelegateProperty =
			FindFProperty<FMulticastDelegateProperty>(AActor::StaticClass(), PropertyName);
		if (!Test.TestNotNull(*FString::Printf(TEXT("BlueprintImpact.BuildImpactSymbols should find delegate property %s"), *PropertyName.ToString()), DelegateProperty))
		{
			return nullptr;
		}

		UDelegateFunction* SignatureFunction = Cast<UDelegateFunction>(DelegateProperty->SignatureFunction.Get());
		if (!Test.TestNotNull(*FString::Printf(TEXT("BlueprintImpact.BuildImpactSymbols should expose a delegate signature for %s"), *PropertyName.ToString()), SignatureFunction))
		{
			return nullptr;
		}

		return SignatureFunction;
	}
}

#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestTrue(...) Test.TestTrue(__VA_ARGS__)

static bool RunBuildImpactSymbolsDelegateFiltering(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactScannerCoreTests_Private;
	UDelegateFunction* SignatureFunction = FindActorDelegateSignature(
		Test,
		GET_MEMBER_NAME_CHECKED(AActor, OnActorBeginOverlap));
	if (SignatureFunction == nullptr)
	{
		return false;
	}

	TSharedRef<FAngelscriptModuleDesc> Module = MakeShared<FAngelscriptModuleDesc>();
	Module->ModuleName = TEXT("BlueprintImpact.ScannerCore");
	Module->Classes.Add(MakeClassDesc(TEXT("ValidActor"), AActor::StaticClass()));
	Module->Classes.Add(MakeClassDesc(TEXT("NullActor"), nullptr));
	Module->Classes.Add(MakeStructDesc(TEXT("ValidStruct"), TBaseStructure<FVector>::Get()));
	Module->Classes.Add(MakeStructDesc(TEXT("NullStruct"), nullptr));
	Module->Enums.Add(MakeEnumDesc(TEXT("ValidEnum"), StaticEnum<EAutoReceiveInput::Type>()));
	Module->Enums.Add(MakeEnumDesc(TEXT("NullEnum"), nullptr));
	Module->Delegates.Add(MakeDelegateDesc(TEXT("ValidDelegate"), SignatureFunction));
	Module->Delegates.Add(MakeDelegateDesc(TEXT("NullDelegate"), nullptr));

	const AngelscriptEditor::BlueprintImpact::FBlueprintImpactSymbols Symbols =
		AngelscriptEditor::BlueprintImpact::BuildImpactSymbols({ Module });

	if (!TestEqual(TEXT("BlueprintImpact.BuildImpactSymbols should keep exactly one non-null class"), Symbols.Classes.Num(), 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("BlueprintImpact.BuildImpactSymbols should keep exactly one non-null struct"), Symbols.Structs.Num(), 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("BlueprintImpact.BuildImpactSymbols should keep exactly one non-null enum"), Symbols.Enums.Num(), 1))
	{
		return false;
	}
	if (!TestEqual(TEXT("BlueprintImpact.BuildImpactSymbols should keep exactly one non-null delegate"), Symbols.Delegates.Num(), 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("BlueprintImpact.BuildImpactSymbols should collect the valid class"), Symbols.Classes.Contains(AActor::StaticClass())))
	{
		return false;
	}
	if (!TestTrue(TEXT("BlueprintImpact.BuildImpactSymbols should collect the valid struct"), Symbols.Structs.Contains(TBaseStructure<FVector>::Get())))
	{
		return false;
	}
	if (!TestTrue(TEXT("BlueprintImpact.BuildImpactSymbols should collect the valid enum"), Symbols.Enums.Contains(StaticEnum<EAutoReceiveInput::Type>())))
	{
		return false;
	}
	if (!TestFalse(TEXT("BlueprintImpact.BuildImpactSymbols should skip null class entries"), Symbols.Classes.Contains(nullptr)))
	{
		return false;
	}
	if (!TestFalse(TEXT("BlueprintImpact.BuildImpactSymbols should skip null struct entries"), Symbols.Structs.Contains(nullptr)))
	{
		return false;
	}
	if (!TestFalse(TEXT("BlueprintImpact.BuildImpactSymbols should skip null enum entries"), Symbols.Enums.Contains(nullptr)))
	{
		return false;
	}
	if (!TestFalse(TEXT("BlueprintImpact.BuildImpactSymbols should skip null delegate entries"), Symbols.Delegates.Contains(nullptr)))
	{
		return false;
	}

	return TestTrue(TEXT("BlueprintImpact.BuildImpactSymbols should collect the valid delegate signature"), Symbols.Delegates.Contains(SignatureFunction));
}

static bool RunMatchChangedScriptsEmptyInputReturnsAllModules(FAutomationTestBase& Test)
{
	using namespace AngelscriptEditor_Private_Tests_AngelscriptBlueprintImpactScannerCoreTests_Private;
	const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = {
		MakeScannerCoreModule(TEXT("Gameplay.Enemy"), { TEXT("Scripts/Gameplay/Enemy.as"), TEXT("Scripts/Gameplay/EnemyAbilities.as") }),
		MakeScannerCoreModule(TEXT("Gameplay.Boss"), { TEXT("Scripts/Gameplay/Boss.as") })
	};

	const TArray<FString> EmptyChangedScripts;
	const TArray<TSharedRef<FAngelscriptModuleDesc>> EmptyMatches =
		AngelscriptEditor::BlueprintImpact::FindModulesForChangedScripts(Modules, EmptyChangedScripts);
	if (!TestEqual(
			TEXT("BlueprintImpact.MatchChangedScriptsToModuleSections should return every active module when ChangedScripts is empty"),
			EmptyMatches.Num(),
			Modules.Num()))
	{
		return false;
	}

	for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
	{
		if (!TestEqual(
				*FString::Printf(TEXT("BlueprintImpact.MatchChangedScriptsToModuleSections should preserve module order for empty input at index %d"), ModuleIndex),
				EmptyMatches[ModuleIndex]->ModuleName,
				Modules[ModuleIndex]->ModuleName))
		{
			return false;
		}
	}

	const TArray<FString> BlankOnlyChangedScripts = {
		TEXT(""),
		TEXT("./"),
		TEXT("/")
	};
	const TArray<FString> NormalizedBlankOnlyChangedScripts =
		AngelscriptEditor::BlueprintImpact::NormalizeChangedScriptPaths(BlankOnlyChangedScripts);
	if (!TestEqual(
			TEXT("BlueprintImpact.MatchChangedScriptsToModuleSections should normalize empty and relative-only path markers down to an empty changed-script set"),
			NormalizedBlankOnlyChangedScripts.Num(),
			0))
	{
		return false;
	}

	const TArray<TSharedRef<FAngelscriptModuleDesc>> BlankOnlyMatches =
		AngelscriptEditor::BlueprintImpact::FindModulesForChangedScripts(Modules, BlankOnlyChangedScripts);
	if (!TestEqual(
			TEXT("BlueprintImpact.MatchChangedScriptsToModuleSections should fall back to all modules when every changed-script input normalizes away"),
			BlankOnlyMatches.Num(),
			Modules.Num()))
	{
		return false;
	}

	for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
	{
		if (!TestEqual(
				*FString::Printf(TEXT("BlueprintImpact.MatchChangedScriptsToModuleSections should preserve module order after blank-only inputs normalize away at index %d"), ModuleIndex),
				BlankOnlyMatches[ModuleIndex]->ModuleName,
				Modules[ModuleIndex]->ModuleName))
		{
			return false;
		}
	}

	return true;
}

#undef TestTrue
#undef TestFalse
#undef TestEqual

TEST_CLASS_WITH_FLAGS(FAngelscriptBlueprintImpactBuildImpactSymbolsCoreTests,
	"Angelscript.Editor.BlueprintImpact.BuildImpactSymbols",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CollectsDelegatesAndSkipsNulls)
	{
		ASSERT_THAT(IsTrue(RunBuildImpactSymbolsDelegateFiltering(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptBlueprintImpactMatchChangedScriptsCoreTests,
	"Angelscript.Editor.BlueprintImpact.MatchChangedScriptsToModuleSections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EmptyInputReturnsAllModules)
	{
		ASSERT_THAT(IsTrue(RunMatchChangedScriptsEmptyInputReturnsAllModules(*TestRunner)));
	}
};

#endif
