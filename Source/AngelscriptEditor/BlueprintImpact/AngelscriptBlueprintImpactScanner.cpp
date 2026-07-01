#include "BlueprintImpact/AngelscriptBlueprintImpactScanner.h"

#include "Serialization/ArchiveReplaceObjectRef.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "BlueprintCompilationManager.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "K2Node_EditablePinBase.h"
#include "K2Node_Event.h"
#include "K2Node_MacroInstance.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace
{
#if WITH_DEV_AUTOMATION_TESTS
	TOptional<TArray<FAssetData>> GBlueprintImpactBlueprintAssetsOverride;
#endif

	template <typename T>
	void AddUniqueReason(TArray<AngelscriptEditor::BlueprintImpact::EBlueprintImpactReason>& Reasons, T Reason)
	{
		if (!Reasons.Contains(Reason))
		{
			Reasons.Add(Reason);
		}
	}

	FString NormalizeScriptPath(const FString& InPath)
	{
		FString Normalized = InPath;
		FPaths::NormalizeFilename(Normalized);

		while (Normalized.RemoveFromStart(TEXT("./")))
		{
		}

		while (Normalized.RemoveFromStart(TEXT("/")))
		{
		}

		Normalized.ToLowerInline();
		return Normalized;
	}

	bool MatchesPinType(const FEdGraphPinType& PinType, const AngelscriptEditor::BlueprintImpact::FBlueprintImpactSymbols& Symbols)
	{
		auto MatchesReplacementObject = [&Symbols](UObject* Object) -> bool
		{
			if (Object == nullptr)
			{
				return false;
			}

			if (Symbols.ReplacementObjects.Contains(Object))
			{
				return true;
			}

			for (const TPair<UObject*, UObject*>& ReplacementObject : Symbols.ReplacementObjects)
			{
				if (ReplacementObject.Value == Object)
				{
					return true;
				}
			}

			return false;
		};

		if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
		{
			UScriptStruct* Struct = Cast<UScriptStruct>(PinType.PinSubCategoryObject.Get());
			return Symbols.Structs.Contains(Struct) || MatchesReplacementObject(Struct);
		}

		if (PinType.PinCategory == UEdGraphSchema_K2::PC_Enum || PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
		{
			UEnum* Enum = Cast<UEnum>(PinType.PinSubCategoryObject.Get());
			return Symbols.Enums.Contains(Enum) || MatchesReplacementObject(Enum);
		}

		return false;
	}
}

namespace AngelscriptEditor::BlueprintImpact
{
	bool FBlueprintImpactSymbols::IsEmpty() const
	{
		return Classes.IsEmpty()
			&& Structs.IsEmpty()
			&& Enums.IsEmpty()
			&& Delegates.IsEmpty()
			&& ReplacementObjects.IsEmpty();
	}

	TArray<FString> NormalizeChangedScriptPaths(const TArray<FString>& ChangedScripts)
	{
		TSet<FString> UniquePaths;
		for (const FString& ChangedScript : ChangedScripts)
		{
			const FString Normalized = NormalizeScriptPath(ChangedScript);
			if (!Normalized.IsEmpty())
			{
				UniquePaths.Add(Normalized);
			}
		}

		TArray<FString> Result = UniquePaths.Array();
		Result.Sort();
		return Result;
	}

	TArray<TSharedRef<FAngelscriptModuleDesc>> FindModulesForChangedScripts(const TArray<TSharedRef<FAngelscriptModuleDesc>>& Modules, const TArray<FString>& ChangedScripts)
	{
		const TSet<FString> NormalizedChangedScripts(NormalizeChangedScriptPaths(ChangedScripts));
		if (NormalizedChangedScripts.IsEmpty())
		{
			return Modules;
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> MatchingModules;
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : Modules)
		{
			for (const FAngelscriptModuleDesc::FCodeSection& CodeSection : Module->Code)
			{
				if (NormalizedChangedScripts.Contains(NormalizeScriptPath(CodeSection.RelativeFilename)))
				{
					MatchingModules.Add(Module);
					break;
				}
			}
		}

		return MatchingModules;
	}

	FBlueprintImpactSymbols BuildImpactSymbols(const TArray<TSharedRef<FAngelscriptModuleDesc>>& Modules)
	{
		FBlueprintImpactSymbols Symbols;
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : Modules)
		{
			for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : Module->Classes)
			{
				if (ClassDesc->Class != nullptr)
				{
					Symbols.Classes.Add(ClassDesc->Class);
				}

				if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(ClassDesc->Struct))
				{
					Symbols.Structs.Add(ScriptStruct);
				}
			}

			for (const TSharedRef<FAngelscriptEnumDesc>& EnumDesc : Module->Enums)
			{
				if (EnumDesc->Enum != nullptr)
				{
					Symbols.Enums.Add(EnumDesc->Enum);
				}
			}

			for (const TSharedRef<FAngelscriptDelegateDesc>& DelegateDesc : Module->Delegates)
			{
				if (DelegateDesc->Function != nullptr)
				{
					Symbols.Delegates.Add(DelegateDesc->Function);
				}
			}
		}

		return Symbols;
	}

	bool AnalyzeLoadedBlueprint(UBlueprint& Blueprint, const FBlueprintImpactSymbols& Symbols, TArray<EBlueprintImpactReason>& OutReasons)
	{
		OutReasons.Reset();
		if (Symbols.IsEmpty())
		{
			return false;
		}

		if (UClass* ParentClass = Blueprint.ParentClass)
		{
			for (UClass* ImpactedClass : Symbols.Classes)
			{
				if (ImpactedClass != nullptr && ParentClass->IsChildOf(ImpactedClass))
				{
					AddUniqueReason(OutReasons, EBlueprintImpactReason::ScriptParentClass);
					break;
				}
			}
		}

		TArray<UK2Node*> AllNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(&Blueprint, AllNodes);
		for (UK2Node* Node : AllNodes)
		{
			TArray<UStruct*> Dependencies;
			if (Node->HasExternalDependencies(&Dependencies))
			{
				for (UStruct* Dependency : Dependencies)
				{
					if (Symbols.Classes.Contains(Cast<UClass>(Dependency)) || Symbols.Structs.Contains(Cast<UScriptStruct>(Dependency)))
					{
						AddUniqueReason(OutReasons, EBlueprintImpactReason::NodeDependency);
						break;
					}
					// UE 5.7: UK2Node_CallFunction::HasExternalDependencies adds
					// the UFunction* (not the owning UClass*) to the output array.
					// Resolve through the function's owner class.
					if (UFunction* DepFunc = Cast<UFunction>(Dependency))
					{
						if (UClass* OwnerClass = DepFunc->GetOwnerClass())
						{
							if (Symbols.Classes.Contains(OwnerClass))
							{
								AddUniqueReason(OutReasons, EBlueprintImpactReason::NodeDependency);
								break;
							}
						}
					}
				}
			}

			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (MatchesPinType(Pin->PinType, Symbols))
				{
					AddUniqueReason(OutReasons, EBlueprintImpactReason::PinType);
					break;
				}
			}

			if (UK2Node_EditablePinBase* EditableBase = Cast<UK2Node_EditablePinBase>(Node))
			{
				for (const TSharedPtr<FUserPinInfo>& PinInfo : EditableBase->UserDefinedPins)
				{
					if (PinInfo.IsValid() && MatchesPinType(PinInfo->PinType, Symbols))
					{
						AddUniqueReason(OutReasons, EBlueprintImpactReason::PinType);
						break;
					}
				}
			}

			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				if (UDelegateFunction* SignatureFunction = Cast<UDelegateFunction>(EventNode->FindEventSignatureFunction()))
				{
					if (Symbols.Delegates.Contains(SignatureFunction))
					{
						AddUniqueReason(OutReasons, EBlueprintImpactReason::DelegateSignature);
					}
				}
			}

			if (UK2Node_MacroInstance* MacroNode = Cast<UK2Node_MacroInstance>(Node))
			{
				if (MatchesPinType(MacroNode->ResolvedWildcardType, Symbols))
				{
					AddUniqueReason(OutReasons, EBlueprintImpactReason::PinType);
				}
			}
		}

		for (const FBPVariableDescription& Variable : Blueprint.NewVariables)
		{
			if (MatchesPinType(Variable.VarType, Symbols))
			{
				AddUniqueReason(OutReasons, EBlueprintImpactReason::VariableType);
				break;
			}
		}

		if (!Symbols.ReplacementObjects.IsEmpty())
		{
			FArchiveReplaceObjectRef<UObject> ReplaceObjectArchive(
				&Blueprint,
				Symbols.ReplacementObjects,
				EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
			if (ReplaceObjectArchive.GetCount() > 0)
			{
				AddUniqueReason(OutReasons, EBlueprintImpactReason::ReferencedAsset);
			}
		}

		return OutReasons.Num() > 0;
	}

	TArray<FAssetData> FindBlueprintAssets(IAssetRegistry& AssetRegistry, const bool bIncludeOnlyOnDiskAssets)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GBlueprintImpactBlueprintAssetsOverride.IsSet())
		{
			return GBlueprintImpactBlueprintAssetsOverride.GetValue();
		}
#endif

		FARFilter Filter;
		Filter.ClassPaths.Add(FTopLevelAssetPath(UBlueprint::StaticClass()));
		Filter.bRecursiveClasses = true;

		TArray<FAssetData> Assets;
		AssetRegistry.GetAssets(Filter, Assets, true);
		if (!bIncludeOnlyOnDiskAssets)
		{
			return Assets;
		}

		TArray<FAssetData> FilteredAssets;
		for (const FAssetData& Asset : Assets)
		{
			FString PackageFilename;
			if (!Asset.PackageName.IsNone() && FPackageName::DoesPackageExist(Asset.PackageName.ToString(), &PackageFilename))
			{
				FilteredAssets.Add(Asset);
			}
		}

		return FilteredAssets;
	}

	FBlueprintImpactScanResult ScanBlueprintAssets(const FAngelscriptEngine& Engine, IAssetRegistry& AssetRegistry, const FBlueprintImpactRequest& Request)
	{
		FBlueprintImpactScanResult Result;
		Result.NormalizedChangedScripts = NormalizeChangedScriptPaths(Request.ChangedScripts);

		const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
		Result.MatchingModules = Request.IsFullScan()
			? ActiveModules
			: FindModulesForChangedScripts(ActiveModules, Result.NormalizedChangedScripts);

		Result.Symbols = BuildImpactSymbols(Result.MatchingModules);
		Result.CandidateAssets = FindBlueprintAssets(AssetRegistry, Request.bIncludeOnlyOnDiskAssets);

		for (const FAssetData& AssetData : Result.CandidateAssets)
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
			if (Blueprint == nullptr)
			{
				++Result.FailedAssetLoads;
				continue;
			}

			FBlueprintImpactMatch Match;
			Match.Blueprint = Blueprint;
			Match.AssetData = AssetData;
			if (AnalyzeLoadedBlueprint(*Blueprint, Result.Symbols, Match.Reasons))
			{
				Result.Matches.Add(Match);
			}
		}

		return Result;
	}

	const TCHAR* LexToString(const EBlueprintImpactReason Reason)
	{
		switch (Reason)
		{
		case EBlueprintImpactReason::ScriptParentClass:
			return TEXT("ScriptParentClass");
		case EBlueprintImpactReason::NodeDependency:
			return TEXT("NodeDependency");
		case EBlueprintImpactReason::PinType:
			return TEXT("PinType");
		case EBlueprintImpactReason::VariableType:
			return TEXT("VariableType");
		case EBlueprintImpactReason::DelegateSignature:
			return TEXT("DelegateSignature");
		case EBlueprintImpactReason::ReferencedAsset:
			return TEXT("ReferencedAsset");
		default:
			return TEXT("Unknown");
		}
	}

#if WITH_DEV_AUTOMATION_TESTS
	void SetBlueprintAssetsOverrideForTesting(TArray<FAssetData> InAssets)
	{
		GBlueprintImpactBlueprintAssetsOverride = MoveTemp(InAssets);
	}

	void ClearBlueprintAssetsOverrideForTesting()
	{
		GBlueprintImpactBlueprintAssetsOverride.Reset();
	}
#endif
}
