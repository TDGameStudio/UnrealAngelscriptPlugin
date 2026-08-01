#include "Contract/AngelscriptOfflineIndices.h"
#include "Compiler/Frontend/AngelscriptStandaloneSource.h"
#include "Resources/AngelscriptAssetIndex.h"
#include "Resources/AngelscriptAssetPath.h"
#include "Resources/AngelscriptResourceContext.h"
#include "Resources/AngelscriptResourceDiagnostics.h"
#include "Resources/AngelscriptResourceValidator.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	using namespace AngelscriptStandalone;
	using namespace AngelscriptStandalone::Frontend;

	bool Require(
		const bool bCondition,
		const std::string_view Message)
	{
		if (!bCondition)
			std::cerr << Message << '\n';
		return bCondition;
	}

	FOfflineSymbolRecord MakeType(
		const char Fill,
		std::string Name,
		std::string UnrealPath)
	{
		FOfflineSymbolRecord Result;
		Result.StableId = std::string(64, Fill);
		Result.Kind = "type";
		Result.Type.StableId = Result.StableId;
		Result.Type.Kind = "reference";
		Result.Type.Name = std::move(Name);
		Result.Type.UETypePath = std::move(UnrealPath);
		return Result;
	}

	FOfflineSymbolRecord MakeCallable(
		const char Fill,
		std::string Name,
		std::string Declaration)
	{
		FOfflineSymbolRecord Result;
		Result.StableId = std::string(64, Fill);
		Result.Kind = "callable";
		Result.Callable.StableId = Result.StableId;
		Result.Callable.Kind = "global-function";
		Result.Callable.Name = std::move(Name);
		Result.Callable.Declaration = std::move(Declaration);
		return Result;
	}
}

int main()
{
	using namespace AngelscriptStandalone;
	using namespace AngelscriptStandalone::Frontend;
	bool bPassed = true;

	const FAssetPathResult Wrapped =
		NormalizeAssetPath(
			"Texture2D'/Game/Textures/T_Icon.T_Icon'");
	bPassed &= Require(
		Wrapped.bSuccess
			&& Wrapped.Normalized
				== "/Game/Textures/T_Icon.T_Icon"
			&& Wrapped.MountPoint == "/Game"
			&& Wrapped.bChanged,
		"wrapped object path did not normalize");
	bPassed &= Require(
		NormalizeAssetPath(
			"/Game/Blueprints/BP_Item.BP_Item_C").Kind
			== EAssetPathKind::GeneratedClass,
		"generated class path was not distinguished");
	bPassed &= Require(
		!NormalizeAssetPath("../Game/Bad").bSuccess,
		"escaping asset path was accepted");
	bPassed &= Require(
		NormalizeAssetPath("/Engine/EngineResources/DefaultTexture.DefaultTexture")
				.MountPoint == "/Engine"
			&& NormalizeAssetPath("/Script/Engine.Texture2D").Kind
				== EAssetPathKind::ScriptObject
			&& NormalizeAssetPath("/ExamplePlugin/Items/P_Item.P_Item")
				.MountPoint == "/ExamplePlugin",
		"engine, script, or plugin mount normalization is incorrect");

	FOfflineAssetRecord Texture;
	Texture.StableId = std::string(64, '1');
	Texture.PackagePath = "/Game/Textures";
	Texture.ObjectPath = "/Game/Textures/T_Icon.T_Icon";
	Texture.AssetClassPath = "/Script/Engine.Texture2D";
	Texture.MountPoint = "/Game";
	Texture.RedirectSource =
		"/Game/Textures/T_Old.T_Old";
	Texture.RedirectTarget = Texture.ObjectPath;
	FOfflineAssetRecord Blueprint;
	Blueprint.StableId = std::string(64, '2');
	Blueprint.PackagePath = "/Game/Blueprints";
	Blueprint.ObjectPath =
		"/Game/Blueprints/BP_Item.BP_Item";
	Blueprint.GeneratedClassPath =
		"/Game/Blueprints/BP_Item.BP_Item_C";
	Blueprint.AssetClassPath =
		"/Script/Engine.Blueprint";
	Blueprint.BaseClassPath = "/Script/Engine.Actor";
	Blueprint.MountPoint = "/Game";
	FOfflineScope CompleteScope;
	CompleteScope.bComplete = true;
	CompleteScope.State = "asset-registry-complete";
	CompleteScope.Included = {"/Game"};
	std::string Error;
	const auto AssetIndex = FAssetIndex::Build(
		{Texture, Blueprint},
		CompleteScope,
		Error);
	bPassed &= Require(
		AssetIndex != nullptr,
		Error);
	if (AssetIndex == nullptr)
		return 1;
	const FAssetLookup Redirect =
		AssetIndex->Lookup("/Game/Textures/T_Old.T_Old");
	bPassed &= Require(
		Redirect.Asset != nullptr
			&& Redirect.bRedirected
			&& Redirect.FinalPath == Texture.ObjectPath,
		"redirect chain did not resolve to the final asset");
	bPassed &= Require(
		AssetIndex->Lookup(
			"/Game/Missing.Missing").bAuthoritativelyCovered,
		"complete /Game scope did not prove membership");

	FOfflineAssetRecord CycleA = Texture;
	CycleA.StableId = std::string(64, '3');
	CycleA.ObjectPath = "/Game/Cycle/A.A";
	CycleA.RedirectSource = "/Game/Cycle/OldA.OldA";
	CycleA.RedirectTarget = "/Game/Cycle/OldB.OldB";
	FOfflineAssetRecord CycleB = Blueprint;
	CycleB.StableId = std::string(64, '4');
	CycleB.ObjectPath = "/Game/Cycle/B.B";
	CycleB.GeneratedClassPath.clear();
	CycleB.RedirectSource = "/Game/Cycle/OldB.OldB";
	CycleB.RedirectTarget = "/Game/Cycle/OldA.OldA";
	bPassed &= Require(
		FAssetIndex::Build(
			{CycleA, CycleB},
			CompleteScope,
			Error) == nullptr
			&& Error.find("cycle") != std::string::npos,
		"redirect cycle was accepted");

	FOfflineSymbolRecord SoftPath =
		MakeType('5', "FSoftObjectPath", {});
	FOfflineSymbolRecord SoftClassPath =
		MakeType('8', "FSoftClassPath", {});
	FOfflineSymbolRecord TextureType =
		MakeType('6', "UTexture2D", "/Script/Engine.Texture2D");
	FOfflineSymbolRecord ActorType =
		MakeType('7', "AActor", "/Script/Engine.Actor");
	FOfflineSymbolRecord LoadObject = MakeCallable(
		'9',
		"LoadObject",
		"UObject LoadObject(UObject Outer, const FString&inout Name)");
	LoadObject.Callable.Parameters.resize(2);
	LoadObject.Callable.Parameters[0].Name = "Outer";
	LoadObject.Callable.Parameters[0].TypeDeclaration = "UObject";
	LoadObject.Callable.Parameters[1].Name = "Name";
	LoadObject.Callable.Parameters[1].TypeDeclaration =
		"const FString&inout";
	LoadObject.Callable.Parameters[1].ResourceKind = "load-object";
	FOfflineSymbolRecord ResolvePreview = MakeCallable(
		'a',
		"ResolvePreview",
		"UObject ResolvePreview(const FString&in ResourcePath)");
	ResolvePreview.Callable.Parameters.resize(1);
	ResolvePreview.Callable.Parameters[0].Name = "ResourcePath";
	ResolvePreview.Callable.Parameters[0].TypeDeclaration =
		"const FString&in";
	ResolvePreview.Callable.Parameters[0].ResourceKind =
		"soft-object";
	ResolvePreview.Callable.Parameters[0].ResourceTypeStableId =
		TextureType.StableId;
	FOfflineSymbolRecord ShowMessage = MakeCallable(
		'b',
		"ShowMessage",
		"void ShowMessage(const FString&in Message)");
	ShowMessage.Callable.Parameters.resize(1);
	ShowMessage.Callable.Parameters[0].Name = "Message";
	ShowMessage.Callable.Parameters[0].TypeDeclaration =
		"const FString&in";
	std::string SymbolError;
	const auto Symbols = FOfflineBundleIndices::Build(
		{
			SoftPath,
			SoftClassPath,
			TextureType,
			ActorType,
			LoadObject,
			ResolvePreview,
			ShowMessage,
		},
		{},
		{},
		SymbolError);
	bPassed &= Require(Symbols != nullptr, SymbolError);
	if (Symbols == nullptr)
		return 1;

	FLanguageModule Module;
	Module.LogicalPath = "Game/Resources.as";
	Module.ModuleId = MakeStableModuleId(Module.LogicalPath);
	Module.ProcessedSource =
		"const FString Root = \"/Game/Textures/\";\n"
		"const FString IconPath = Root + \"T_Icon.T_Icon\";\n"
		"FSoftObjectPath Icon(IconPath);\n"
		"FSoftClassPath ItemClass("
			"\"/Game/Blueprints/BP_Item.BP_Item_C\");\n"
		"UObject Loaded = LoadObject(nullptr, IconPath);\n"
		"FString MutablePath = \"/Game/Missing.Missing\";\n"
		"FSoftObjectPath Dynamic(MutablePath);\n"
		"string Ordinary = \"/Game/Missing.Missing\";\n";
	const FResourceContextResult Contexts =
		DiscoverResourceContexts({Module}, {}, *Symbols);
	if (Contexts.Contexts.size() != 3)
	{
		std::cerr << "resource context count="
			<< Contexts.Contexts.size()
			<< " error=" << Contexts.Error << '\n';
		for (const auto& Context : Contexts.Contexts)
			std::cerr << "  " << Context.ConstantPath << '\n';
	}
	bPassed &= Require(
		Contexts.bSuccess
			&& Contexts.Contexts.size() == 3,
		"typed context discovery did not resolve the constructor, "
			"class path, and marked load call exactly once each");
	const std::size_t IconContextCount = std::count_if(
		Contexts.Contexts.begin(),
		Contexts.Contexts.end(),
		[&Texture](const FResourceContext& Context)
		{
			return Context.ConstantPath == Texture.ObjectPath;
		});
	bPassed &= Require(
		IconContextCount == 2,
		"bounded const concatenation was not resolved for both "
			"the soft-path constructor and LoadObject call");
	bPassed &= Require(
		std::none_of(
			Contexts.Contexts.begin(),
			Contexts.Contexts.end(),
			[](const FResourceContext& Context)
			{
				return Context.ConstantPath
					== "/Game/Missing.Missing";
			}),
		"mutable or ordinary string data was treated as a resource");
	const std::vector<FSemanticObservation> NoSemanticMatches;
	bPassed &= Require(
		DiscoverResourceContexts(
			{Module},
			{},
			*Symbols,
			&NoSemanticMatches).Contexts.empty(),
		"resource calls were accepted without resolved compiler evidence");
	std::vector<FSemanticObservation> ResolvedSemantics;
	auto AddConstructorEvidence =
		[&](const std::string_view Name,
			const std::string& StableTypeId)
		{
			FSemanticObservation Observation;
			Observation.Kind =
				ESemanticObservationKind::Constructor;
			Observation.LogicalPath = Module.LogicalPath;
			// The maintained fork can report a zero constructor call span
			// while still reporting exact argument spans. Resource identity
			// must accept that bounded compiler evidence.
			Observation.Source = {0, 0};
			Observation.TargetStableTypeId = StableTypeId;
			const std::size_t Constructor =
				Module.ProcessedSource.find(Name);
			const std::size_t Open =
				Module.ProcessedSource.find('(', Constructor);
			const std::size_t Close =
				Module.ProcessedSource.find(')', Open);
			Observation.Arguments.resize(1);
			Observation.Arguments[0].Source = {
				Open + 1,
				Close,
			};
			ResolvedSemantics.push_back(std::move(Observation));
		};
	AddConstructorEvidence(
		"FSoftObjectPath",
		SoftPath.StableId);
	AddConstructorEvidence(
		"FSoftClassPath",
		SoftClassPath.StableId);
	FSemanticObservation LoadEvidence;
	LoadEvidence.Kind =
		ESemanticObservationKind::ResolvedCall;
	LoadEvidence.LogicalPath = Module.LogicalPath;
	LoadEvidence.Source.Begin =
		Module.ProcessedSource.find("LoadObject");
	LoadEvidence.Source.End =
		LoadEvidence.Source.Begin
			+ std::string_view("LoadObject").size();
	LoadEvidence.StableFunctionId = LoadObject.StableId;
	ResolvedSemantics.push_back(std::move(LoadEvidence));
	bPassed &= Require(
		DiscoverResourceContexts(
			{Module},
			{},
			*Symbols,
			&ResolvedSemantics).Contexts.size() == 3,
		"resolved semantic stable IDs did not authorize the "
			"three typed resource contexts");

	FLanguageModule MarkedModule;
	MarkedModule.LogicalPath = "Game/MarkedResource.as";
	MarkedModule.ModuleId =
		MakeStableModuleId(MarkedModule.LogicalPath);
	MarkedModule.ProcessedSource =
		"const FString PreviewPath = "
			"\"/Game/Textures/T_Icon.T_Icon\";\n"
		"UObject Preview = ResolvePreview(PreviewPath);\n"
		"ShowMessage(PreviewPath);\n";
	const std::size_t MarkedArgument =
		MarkedModule.ProcessedSource.find(
			"PreviewPath);",
			MarkedModule.ProcessedSource.find("ResolvePreview"));
	const std::size_t UnmarkedArgument =
		MarkedModule.ProcessedSource.find(
			"PreviewPath);",
			MarkedModule.ProcessedSource.find("ShowMessage"));
	FSemanticObservation MarkedObservation;
	MarkedObservation.Kind =
		ESemanticObservationKind::ResolvedCall;
	MarkedObservation.LogicalPath = MarkedModule.LogicalPath;
	MarkedObservation.StableFunctionId =
		ResolvePreview.StableId;
	MarkedObservation.Arguments.resize(1);
	MarkedObservation.Arguments[0].Source = {
		MarkedArgument,
		MarkedArgument + std::string_view("PreviewPath").size(),
	};
	FSemanticObservation UnmarkedObservation;
	UnmarkedObservation.Kind =
		ESemanticObservationKind::ResolvedCall;
	UnmarkedObservation.LogicalPath =
		MarkedModule.LogicalPath;
	UnmarkedObservation.StableFunctionId =
		ShowMessage.StableId;
	UnmarkedObservation.Arguments.resize(1);
	UnmarkedObservation.Arguments[0].Source = {
		UnmarkedArgument,
		UnmarkedArgument + std::string_view("PreviewPath").size(),
	};
	const std::vector<FSemanticObservation> MarkedObservations = {
		MarkedObservation,
		UnmarkedObservation,
	};
	const FResourceContextResult MarkedContexts =
		DiscoverResourceContexts(
			{MarkedModule},
			{},
			*Symbols,
			&MarkedObservations);
	bPassed &= Require(
		MarkedContexts.bSuccess
			&& MarkedContexts.Contexts.size() == 1
			&& MarkedContexts.Contexts[0].ContextStableSymbolId
				== ResolvePreview.StableId
			&& MarkedContexts.Contexts[0].RequestedStableTypeId
				== TextureType.StableId
			&& MarkedContexts.Contexts[0].ConstantPath
				== Texture.ObjectPath,
		"bundle-marked callable parameter was not resolved by "
			"stable callable identity, or an unmarked string parameter "
			"was treated as a resource");
	if (!Contexts.Contexts.empty())
	{
		const auto IconContext = std::find_if(
			Contexts.Contexts.begin(),
			Contexts.Contexts.end(),
			[&Texture](const FResourceContext& Context)
			{
				return Context.bSoft
					&& !Context.bClass
					&& Context.ConstantPath == Texture.ObjectPath;
			});
		bPassed &= Require(
			IconContext != Contexts.Contexts.end(),
			"soft object constructor context was not retained");
		if (IconContext == Contexts.Contexts.end())
			return 1;
		FResourceValidation Found = ValidateResourceContext(
			*IconContext,
			*AssetIndex,
			*Symbols);
		bPassed &= Require(
			Found.State == EResourceState::Found,
			"known soft object path was not found");

		FResourceContext Missing = *IconContext;
		Missing.ConstantPath = "/Game/Missing.Missing";
		const FResourceValidation MissingResult =
			ValidateResourceContext(
				Missing,
				*AssetIndex,
				*Symbols);
		bPassed &= Require(
			MissingResult.State == EResourceState::Missing
				&& !IsResourceFailure(MissingResult, false)
				&& IsResourceFailure(MissingResult, true),
			"soft missing/strict resource policy is incorrect");

		FResourceContext Incompatible = *IconContext;
		Incompatible.RequestedStableTypeId =
			ActorType.StableId;
		const FResourceValidation IncompatibleResult =
			ValidateResourceContext(
				Incompatible,
				*AssetIndex,
				*Symbols);
		bPassed &= Require(
			IncompatibleResult.State
					== EResourceState::Incompatible
				&& MakeResourceDiagnostic(
					IncompatibleResult,
					false).Severity
					== AngelscriptStandalone::EDiagnosticSeverity::Error,
			"known incompatible resource type did not fail");

		FOfflineScope IncompleteScope = CompleteScope;
		IncompleteScope.bComplete = false;
		const auto IncompleteIndex = FAssetIndex::Build(
			{Texture},
			IncompleteScope,
			Error);
		bPassed &= Require(
			ValidateResourceContext(
				Missing,
				*IncompleteIndex,
				*Symbols).State == EResourceState::Unknown,
			"incomplete scope fabricated a missing result");
	}

	FLanguageModule ShadowModule;
	ShadowModule.LogicalPath = "Game/ShadowedLoad.as";
	ShadowModule.ModuleId =
		MakeStableModuleId(ShadowModule.LogicalPath);
	ShadowModule.ProcessedSource =
		"const FString Path = \"/Game/Textures/T_Icon.T_Icon\";\n"
		"UObject LoadObject(UObject Outer, const FString&in Name)"
			" { return Outer; }\n"
		"UObject Value = LoadObject(nullptr, Path);\n";
	FDeclaration ShadowDeclaration;
	ShadowDeclaration.Kind = EDeclarationKind::Function;
	ShadowDeclaration.Name = "LoadObject";
	ShadowDeclaration.ModuleId = ShadowModule.ModuleId;
	ShadowDeclaration.Source.LogicalPath = ShadowModule.LogicalPath;
	const FResourceContextResult Shadowed =
		DiscoverResourceContexts(
			{ShadowModule},
			{ShadowDeclaration},
			*Symbols);
	bPassed &= Require(
		Shadowed.bSuccess && Shadowed.Contexts.empty(),
		"a source-defined same-named function was mistaken for "
			"the bundle LoadObject symbol");

	return bPassed ? 0 : 1;
}
