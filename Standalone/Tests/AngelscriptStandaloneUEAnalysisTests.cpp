#include "Compiler/AngelscriptStandaloneDeclarationAnalysis.h"
#include "Compiler/AngelscriptStandaloneSourceGraph.h"
#include "Compiler/AngelscriptStandaloneNativeCompiler.h"
#include "Compiler/AngelscriptStandaloneUECompiler.h"
#include "Compiler/AngelscriptValidationArtifact.h"
#include "Contract/AngelscriptBundleTypeOracle.h"
#include "Contract/AngelscriptOfflineIndices.h"
#include "Support/AngelscriptStandaloneHash.h"
#include "Compiler/Frontend/AngelscriptStandaloneSource.h"
#include "Registration/AngelscriptRegistrationPlan.h"
#include "Registration/AngelscriptRegistrationLoader.h"
#include "Registration/AngelscriptScriptBaselinePlan.h"
#include "Registration/AngelscriptCapabilityClassification.h"
#include "Registration/AngelscriptCompileOnlyStub.h"

#include "angelscript.h"

#include <chrono>
#include <filesystem>
#include <fstream>
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
		const std::string_view Test,
		const std::string_view Message)
	{
		if (!bCondition)
		{
			std::cerr << Test << ": " << Message << '\n';
		}
		return bCondition;
	}

	std::string ReadFile(const std::filesystem::path& Path)
	{
		std::ifstream Input(Path, std::ios::binary);
		return {
			std::istreambuf_iterator<char>(Input),
			std::istreambuf_iterator<char>(),
		};
	}

	FOfflineSymbolRecord MakeType(
		std::string Id,
		std::string Name,
		std::string Layer = "host-surface",
		std::string Module = {},
		std::string ModuleId = {})
	{
		FOfflineSymbolRecord Record;
		Record.StableId = std::move(Id);
		Record.Kind = "type";
		Record.Type.StableId = Record.StableId;
		Record.Type.Kind = "reference";
		Record.Type.Name = std::move(Name);
		Record.Type.CompleteDeclaration = Record.Type.Name;
		Record.Origin.Layer = std::move(Layer);
		Record.Origin.Kind =
			Record.Origin.Layer == "script-baseline"
				? "script"
				: "manual";
		Record.Origin.Module = std::move(Module);
		Record.Origin.StableModuleId = std::move(ModuleId);
		return Record;
	}

	FOfflineSymbolRecord MakeCallable(
		std::string Id,
		std::string OwnerId)
	{
		FOfflineSymbolRecord Record;
		Record.StableId = std::move(Id);
		Record.Kind = "callable";
		Record.Callable.StableId = Record.StableId;
		Record.Callable.Kind = "method";
		Record.Callable.Name = "Tick";
		Record.Callable.OwnerStableId = std::move(OwnerId);
		Record.Callable.Declaration = "void Tick()";
		Record.Callable.ReturnType = "void";
		Record.Origin.Layer = "host-surface";
		Record.Origin.Kind = "manual";
		return Record;
	}

	bool TestSourceClosure()
	{
		constexpr std::string_view Name = "SourceClosure";
		FStandaloneSourceGraphRequest Request;
		Request.Entry = "Game/Main.as";
		Request.Config.Flags.emplace("EDITOR", false);
		Request.Config.Flags.emplace("TEST", true);
		Request.Sources = {
			{"Unused.as", "import Missing.ButUnused;\n"},
			{"Game/Main.as",
				"#if TEST\nimport Game.Shared;\n#endif\nvoid Main() {}\n"},
			{"Game/Shared.as", "void Shared() {}\n"},
		};
		const FStandaloneSourceGraphResult Result =
			BuildStandaloneSourceGraph(Request);
		bool bPassed = Require(Result.bSuccess, Name, Result.Error);
		bPassed &= Require(
			Result.Modules.size() == 2
				&& Result.Modules[0].ModuleName == "Game.Shared"
				&& Result.Modules[1].ModuleName == "Game.Main",
			Name,
			"selected closure was not dependency-first or included unrelated source");

		Request.Sources.erase(Request.Sources.begin() + 2);
		const auto Missing = BuildStandaloneSourceGraph(Request);
		bPassed &= Require(
			!Missing.bSuccess
				&& !Missing.Diagnostics.empty()
				&& Missing.Diagnostics[0].Code
					== "ASL-PREPROCESS-IMPORT-MISSING",
			Name,
			"missing selected import was not rejected");

		Request.Sources = {
			{"Game/Main.as", "import Game.Shared;\n"},
			{"Game/Shared.as", "import Game.Main;\n"},
		};
		const auto Cycle = BuildStandaloneSourceGraph(Request);
		bPassed &= Require(
			!Cycle.bSuccess
				&& !Cycle.Diagnostics.empty()
				&& Cycle.Diagnostics[0].Code
					== "ASL-PREPROCESS-IMPORT-CYCLE",
			Name,
			"selected import cycle was not rejected");

		Request.Sources = {
			{"Game/Main.as", "void A() {}\n"},
			{"Game\\Main.as", "void B() {}\n"},
		};
		const auto Duplicate = BuildStandaloneSourceGraph(Request);
		bPassed &= Require(
			!Duplicate.bSuccess
				&& Duplicate.Error.find("duplicate") != std::string::npos,
			Name,
			"duplicate normalized module was not rejected");
		return bPassed;
	}

	bool TestFilesystemCollection()
	{
		constexpr std::string_view Name = "FilesystemCollection";
		const auto Unique = std::chrono::steady_clock::now()
			.time_since_epoch()
			.count();
		const std::filesystem::path Root =
			std::filesystem::temp_directory_path()
			/ ("angelscript-source-graph-" + std::to_string(Unique));
		std::filesystem::create_directories(Root / "Game");
		{
			std::ofstream Output(
				Root / "Game" / "Main.as",
				std::ios::binary);
			Output << "\xef\xbb\xbfvoid Main() {}\n";
		}
		const FStandaloneSourceCollectionResult Result =
			CollectStandaloneSources({Root});
		std::error_code ErrorCode;
		std::filesystem::remove_all(Root, ErrorCode);
		return Require(
			Result.bSuccess
				&& Result.Sources.size() == 1
				&& Result.Sources[0].LogicalPath == "Game/Main.as"
				&& Result.Sources[0].Contents == "void Main() {}\n",
			Name,
			Result.Error);
	}

	bool TestBaselineReplacementAndRegistrationPlan()
	{
		constexpr std::string_view Name =
			"BaselineReplacementAndRegistrationPlan";
		const std::string HostTypeId(64, '1');
		const std::string CallableId(64, '2');
		const std::string BaselineAId(64, '3');
		const std::string BaselineBId(64, '4');
		const std::string ModuleAId = AngelscriptStandalone::Sha256(
			"module-id-v1\nGame.A\n/Angelscript/Game/Game/A.as");
		const std::string ModuleBId = AngelscriptStandalone::Sha256(
			"module-id-v1\nGame.B\n/Angelscript/Game/Game/B.as");

		std::vector<FOfflineSymbolRecord> Symbols;
		Symbols.push_back(MakeType(HostTypeId, "FHost"));
		Symbols.push_back(MakeCallable(CallableId, HostTypeId));
		Symbols.push_back(MakeType(
			BaselineAId,
			"AFromBaseline",
			"script-baseline",
			"Game.A",
			ModuleAId));
		Symbols.push_back(MakeType(
			BaselineBId,
			"BFromBaseline",
			"script-baseline",
			"Game.B",
			ModuleBId));
		std::string IndexError;
		const std::shared_ptr<const FOfflineBundleIndices> Indices =
			FOfflineBundleIndices::Build(
				std::move(Symbols),
				{},
				{},
				IndexError);
		if (!Require(Indices != nullptr, Name, IndexError))
		{
			return false;
		}

		FLanguageModule CurrentA;
		CurrentA.LogicalPath = "Game/A.as";
		CurrentA.ModuleName = "Game.A";
		CurrentA.ModuleId = MakeStableModuleId(CurrentA.LogicalPath);
		const FScriptBaselinePlan Baseline =
			BuildScriptBaselinePlan(*Indices, {CurrentA});
		bool bPassed = Require(Baseline.bSuccess, Name, Baseline.Error);
		bPassed &= Require(
			Baseline.IncludedSymbols.size() == 3
				&& Baseline.SuppressedBaselineSymbols.size() == 1
				&& Baseline.SuppressedBaselineSymbols[0]->StableId
					== BaselineAId,
			Name,
			"current source did not exactly replace its baseline module");

		FOfflineManifest Manifest;
		const FRegistrationPlan Plan =
			BuildRegistrationPlan(Manifest, Baseline, {CurrentA});
		bPassed &= Require(Plan.bSuccess, Name, Plan.Error);
		if (Plan.bSuccess)
		{
			bool bSawSettings = false;
			bool bSawSkeleton = false;
			bool bSawMember = false;
			bool bSawSource = false;
			for (const FRegistrationPlanItem& Item : Plan.Items)
			{
				bSawSettings |= Item.Stage
					== ERegistrationStage::EngineSettings;
				bSawSkeleton |= Item.Stage
					== ERegistrationStage::TypeSkeleton;
				bSawMember |= Item.Stage
					== ERegistrationStage::MembersAndGlobals;
				bSawSource |= Item.Stage
					== ERegistrationStage::Sources;
			}
			bPassed &= Require(
				bSawSettings && bSawSkeleton && bSawMember && bSawSource,
				Name,
				"registration plan omitted a required dependency stage");
		}

		std::vector<FOfflineSymbolRecord> BrokenSymbols;
		BrokenSymbols.push_back(MakeCallable(CallableId, std::string(64, 'f')));
		const auto BrokenIndices = FOfflineBundleIndices::Build(
			std::move(BrokenSymbols),
			{},
			{},
			IndexError);
		const FScriptBaselinePlan BrokenBaseline =
			BuildScriptBaselinePlan(*BrokenIndices, {});
		const FRegistrationPlan BrokenPlan =
			BuildRegistrationPlan(Manifest, BrokenBaseline, {});
		bPassed &= Require(
			!BrokenPlan.bSuccess
				&& BrokenPlan.Error.find("missing owner") != std::string::npos,
			Name,
			"registration plan accepted a missing member owner");
		return bPassed;
	}

	bool TestExportedProjectBaselineIdentityReplacement()
	{
		constexpr std::string_view Name =
			"ExportedProjectBaselineIdentityReplacement";
		constexpr std::string_view ExportedModuleId =
			"f94415ed0e255036e8c21d69312ab1b13b0be676c4c56ba2f5b58507b236b610";

		std::vector<FOfflineSymbolRecord> Symbols;
		Symbols.push_back(MakeType(
			std::string(64, '5'),
			"FExternalConsumerSmokeValue",
			"script-baseline",
			"ExternalConsumerSmoke",
			std::string(ExportedModuleId)));
		std::string IndexError;
		const auto Indices = FOfflineBundleIndices::Build(
			std::move(Symbols),
			{},
			{},
			IndexError);
		if (!Require(Indices != nullptr, Name, IndexError))
		{
			return false;
		}

		FLanguageModule Current;
		Current.LogicalPath = "ExternalConsumerSmoke.as";
		Current.ModuleName = "ExternalConsumerSmoke";
		Current.ModuleId = MakeStableModuleId(Current.LogicalPath);
		const FScriptBaselinePlan Baseline =
			BuildScriptBaselinePlan(*Indices, {Current});
		return Require(
			Baseline.bSuccess
				&& Baseline.IncludedSymbols.empty()
				&& Baseline.SuppressedBaselineSymbols.size() == 1
				&& Baseline.ReplacedModuleIds
					== std::vector<std::string>{std::string(ExportedModuleId)},
			Name,
			Baseline.bSuccess
				? "the exported project baseline was not replaced exactly"
				: Baseline.Error);
	}

	bool TestCapabilityPolicyAndCompileOnlyTrap()
	{
		constexpr std::string_view Name =
			"CapabilityPolicyAndCompileOnlyTrap";
		const std::vector<FCapabilityObservation> Observations = {
			{"exact", "type", ECapabilityClassification::Exact, {}},
			{"shim", "layout", ECapabilityClassification::CompileShim, {}},
			{"ue", "cdo", ECapabilityClassification::UERequired, {}},
		};
		const FCapabilitySummary Strict =
			SummarizeCapabilities(Observations, false);
		const FCapabilitySummary Allowed =
			SummarizeCapabilities(Observations, true);
		bool bPassed = Require(
			!Strict.bCanCompile
				&& Strict.Completeness == EValidationCompleteness::Failed,
			Name,
			"ue-required capability did not fail by default");
		bPassed &= Require(
			Allowed.bCanCompile
				&& Allowed.Completeness == EValidationCompleteness::Partial,
			Name,
			"explicitly allowed ue-required capability was not partial");

		auto Unsupported = Observations;
		Unsupported.push_back({
			"unsupported",
			"ffi",
			ECapabilityClassification::Unsupported,
			{},
		});
		bPassed &= Require(
			!SummarizeCapabilities(Unsupported, true).bCanCompile,
			Name,
			"unsupported capability was weakened by allow-ue-required");

		asIScriptEngine* Engine = asCreateScriptEngine();
		if (!Require(Engine != nullptr, Name, "could not create test engine"))
		{
			return false;
		}
		const int Registration = Engine->RegisterGlobalFunction(
			"int Imported()",
			asFUNCTION(CompileOnlyTrap),
			asCALL_GENERIC);
		asIScriptModule* Module =
			Engine->GetModule("trap", asGM_ALWAYS_CREATE);
		const char* Script = "int main() { return Imported(); }";
		const bool bBuilt = Registration >= 0
			&& Module != nullptr
			&& Module->AddScriptSection("trap.as", Script) >= 0
			&& Module->Build() >= 0;
		bPassed &= Require(
			bBuilt,
			Name,
			"compile-only import did not remain compile-visible");
		if (bBuilt)
		{
			asIScriptContext* Context = Engine->CreateContext();
			asIScriptFunction* Function =
				Module->GetFunctionByDecl("int main()");
			const int Outcome =
				Context != nullptr
					&& Function != nullptr
					&& Context->Prepare(Function) >= 0
				? Context->Execute()
				: asEXECUTION_ERROR;
			bPassed &= Require(
				Outcome == asEXECUTION_EXCEPTION
					&& std::string_view(Context->GetExceptionString())
						== CompileOnlyTrapMessage,
				Name,
				"compile-only import fabricated a runtime result");
			if (Context != nullptr)
			{
				Context->Release();
			}
		}
		Engine->ShutDownAndRelease();
		asThreadCleanup();
		return bPassed;
	}

	bool TestAvailabilityPolicyRejectsOnlyUsedUnavailableSymbols()
	{
		constexpr std::string_view Name =
			"AvailabilityPolicyRejectsOnlyUsedUnavailableSymbols";
		const std::string EditorOnlyFunctionId(64, 'a');
		const std::string UnavailableTypeId(64, 'b');
		const std::string UnusedEditorOnlyId(64, 'c');
		const std::string DeprecatedTypeId(64, 'd');
		std::unordered_map<std::string, std::string> Availability = {
			{EditorOnlyFunctionId, "editor-only"},
			{UnavailableTypeId, "unavailable"},
			{UnusedEditorOnlyId, "editor-only"},
			{DeprecatedTypeId, "deprecated"},
		};
		FSemanticObservation Call;
		Call.Kind = ESemanticObservationKind::ResolvedCall;
		Call.StableFunctionId = EditorOnlyFunctionId;
		Call.TargetStableTypeId = DeprecatedTypeId;
		FSemanticArgumentObservation Argument;
		Argument.ParameterStableTypeId = UnavailableTypeId;
		Call.Arguments.push_back(std::move(Argument));

		std::vector<FCapabilityObservation> Capabilities;
		AppendUsedAvailabilityCapabilities(
			{Call, Call},
			Availability,
			Capabilities);
		const FCapabilitySummary Summary =
			SummarizeCapabilities(Capabilities, true);
		return Require(
			Capabilities.size() == 2
				&& Capabilities[0].Classification
					== ECapabilityClassification::Unsupported
				&& Capabilities[1].Classification
					== ECapabilityClassification::Unsupported
				&& Summary.UnsupportedCount == 2
				&& !Summary.bCanCompile,
			Name,
			"used editor-only/unavailable symbols were not rejected "
			"deterministically, or unused/deprecated symbols were rejected");
	}

	bool TestBundleTypeOracleResolvesStableIdentities()
	{
		constexpr std::string_view Name =
			"BundleTypeOracleResolvesStableIdentities";
		FOfflineSymbolRecord Object =
			MakeType(std::string(64, 'd'), "UObject");
		FOfflineSymbolRecord String =
			MakeType(std::string(64, 'e'), "FString");
		String.Type.Kind = "value";
		FOfflineSymbolRecord Array =
			MakeType(std::string(64, 'f'), "TArray");
		Array.Type.Kind = "template";
		Array.Type.bTemplateDefinition = true;
		FOfflineSymbolRecord FooA =
			MakeType(std::string(64, '1'), "FFoo");
		FooA.Type.Namespace = "Alpha";
		FOfflineSymbolRecord FooB =
			MakeType(std::string(64, '2'), "FFoo");
		FooB.Type.Namespace = "Beta";

		std::string Error;
		const auto Indices = FOfflineBundleIndices::Build(
			{
				std::move(Object),
				std::move(String),
				std::move(Array),
				std::move(FooA),
				std::move(FooB),
			},
			{},
			{},
			Error);
		if (!Require(Indices != nullptr, Name, Error))
		{
			return false;
		}
		const FBundleTypeOracle Oracle(*Indices);
		std::string StableId;
		bool bPassed = Require(
			Oracle.ResolveType(" UObject ", StableId)
				&& StableId == std::string(64, 'd'),
			Name,
			"unqualified unique bundle type did not resolve");
		bPassed &= Require(
			Oracle.ResolveType("Alpha :: FFoo", StableId)
				&& StableId == std::string(64, '1'),
			Name,
			"qualified bundle type did not resolve");
		bPassed &= Require(
			!Oracle.ResolveType("FFoo", StableId),
			Name,
			"ambiguous unqualified bundle type was accepted");

		std::vector<FDeclaration> Declarations;
		Declarations.reserve(2);
		FDeclaration& Class = Declarations.emplace_back();
		Class.Kind = EDeclarationKind::Class;
		Class.Name = "UMyObject";
		Class.QualifiedName = "UMyObject";
		Class.Type.StableTypeId = "source-type:test:UMyObject";
		Class.BaseTypes.push_back({"UObject"});
		FDeclaration& Property = Declarations.emplace_back();
		Property.Kind = EDeclarationKind::Property;
		Property.Name = "Names";
		Property.Owner = "UMyObject";
		Property.Type.Spelling = "TArray<FString>";
		const FTypeResolutionResult Resolution =
			ResolveDeclarationTypes(Declarations, Oracle);
		bPassed &= Require(
			Resolution.bSuccess
				&& Class.BaseTypes[0].StableTypeId
					== std::string(64, 'd')
				&& Property.Type.StableTypeId
					== std::string(64, 'f')
						+ ":specialization:TArray<FString>",
			Name,
			"declaration IR did not retain bundle stable type identities");

		FStandaloneSourceGraphRequest GraphRequest;
		GraphRequest.Entry = "Game/Widget.as";
		GraphRequest.Sources = {{
			"Game/Widget.as",
			"UCLASS()\n"
			"class UWidget : UObject\n"
			"{\n"
			"  UPROPERTY(Replicated)\n"
			"  FString Name;\n"
			"  UFUNCTION(BlueprintEvent)\n"
			"  void OnName(FString Value);\n"
			"}\n",
		}};
		FStandaloneSourceGraphResult Graph =
			BuildStandaloneSourceGraph(GraphRequest);
		bPassed &= Require(Graph.bSuccess, Name, Graph.Error);
		if (Graph.bSuccess)
		{
			const FDeclarationAnalysisResult Analysis =
				AnalyzeStandaloneDeclarations(Graph.Modules, Oracle);
			bPassed &= Require(
				Analysis.bSuccess
					&& Analysis.Declarations.size() == 3,
				Name,
				Analysis.Error);
			bool bSawResolvedBase = false;
			bool bSawResolvedProperty = false;
			bool bSawUERequired = false;
			bool bSawCompileShim = false;
			for (const FDeclaration& Declaration
				: Analysis.Declarations)
			{
				bSawResolvedBase |= Declaration.Kind
						== EDeclarationKind::Class
					&& Declaration.BaseTypes.size() == 1
					&& Declaration.BaseTypes[0].StableTypeId
						== std::string(64, 'd');
				bSawResolvedProperty |= Declaration.Kind
						== EDeclarationKind::Property
					&& Declaration.Type.StableTypeId
						== std::string(64, 'e');
			}
			for (const FCapabilityObservation& Capability
				: Analysis.Capabilities)
			{
				bSawUERequired |= Capability.Classification
					== ECapabilityClassification::UERequired;
				bSawCompileShim |= Capability.Classification
					== ECapabilityClassification::CompileShim;
			}
			bPassed &= Require(
				bSawResolvedBase
					&& bSawResolvedProperty
					&& bSawUERequired
					&& bSawCompileShim,
				Name,
				"declaration semantic analysis omitted stable identities "
					"or support classifications");
			bPassed &= Require(
				Graph.Modules[0].ProcessedSource.find(
					"void OnName(FString Value){}")
					!= std::string::npos,
				Name,
				"declaration-only UE event was not lowered for compilation");
		}
		return bPassed;
	}

	bool TestRegistrationLoaderCompilesAndMapsStableSymbols()
	{
		constexpr std::string_view Name =
			"RegistrationLoaderCompilesAndMapsStableSymbols";
		const std::string EnumId(64, '5');
		const std::string ValueId(64, '6');
		const std::string ObjectId(64, '7');
		const std::string MethodId(64, '8');
		const std::string ObjectPropertyId(64, '9');
		const std::string GlobalFunctionId(64, 'a');
		const std::string GlobalPropertyId(64, 'b');

		FOfflineSymbolRecord Enum = MakeType(EnumId, "EMode");
		Enum.Type.Kind = "enum";
		Enum.Type.EnumValues.push_back({std::string(64, 'c'), "Default", 0});
		FOfflineSymbolRecord Value = MakeType(ValueId, "FVector");
		Value.Type.Kind = "value";
		Value.Type.CompileSize = 12;
		Value.Type.CompileAlignment = 4;
		FOfflineSymbolRecord Object = MakeType(ObjectId, "UObject");
		Object.Type.Kind = "reference";
		FOfflineSymbolRecord Method = MakeCallable(MethodId, ObjectId);
		Method.Callable.Name = "Ping";
		Method.Callable.Declaration = "void Ping()";
		FOfflineSymbolRecord ObjectProperty;
		ObjectProperty.StableId = ObjectPropertyId;
		ObjectProperty.Kind = "property";
		ObjectProperty.Property.StableId = ObjectPropertyId;
		ObjectProperty.Property.OwnerStableId = ObjectId;
		ObjectProperty.Property.Name = "Value";
		ObjectProperty.Property.TypeDeclaration = "int";
		ObjectProperty.Property.Declaration = "int Value";
		ObjectProperty.Origin.Layer = "host-surface";
		ObjectProperty.Origin.Kind = "manual";
		FOfflineSymbolRecord GlobalFunction =
			MakeCallable(GlobalFunctionId, {});
		GlobalFunction.Callable.Kind = "global-function";
		GlobalFunction.Callable.Name = "GetValue";
		GlobalFunction.Callable.Declaration = "int GetValue()";
		GlobalFunction.Callable.ReturnType = "int";
		FOfflineSymbolRecord GlobalProperty = ObjectProperty;
		GlobalProperty.StableId = GlobalPropertyId;
		GlobalProperty.Property.StableId = GlobalPropertyId;
		GlobalProperty.Property.OwnerStableId.clear();
		GlobalProperty.Property.Name = "GValue";
		GlobalProperty.Property.Declaration = "int GValue";

		std::string Error;
		const auto Indices = FOfflineBundleIndices::Build(
			{
				std::move(Enum),
				std::move(Value),
				std::move(Object),
				std::move(Method),
				std::move(ObjectProperty),
				std::move(GlobalFunction),
				std::move(GlobalProperty),
			},
			{},
			{},
			Error);
		if (!Require(Indices != nullptr, Name, Error))
		{
			return false;
		}
		const FScriptBaselinePlan Baseline =
			BuildScriptBaselinePlan(*Indices, {});
		FOfflineManifest Manifest;
		const FRegistrationPlan Plan =
			BuildRegistrationPlan(Manifest, Baseline, {});
		if (!Require(Plan.bSuccess, Name, Plan.Error))
		{
			return false;
		}

		asIScriptEngine* Engine = asCreateScriptEngine();
		if (!Require(Engine != nullptr, Name, "could not create test engine"))
		{
			return false;
		}
		const FRegistrationLoadResult Loaded =
			ApplyRegistrationPlan(Engine, Plan, Manifest);
		bool bPassed = Require(Loaded.bSuccess, Name, Loaded.Error);
		if (Loaded.bSuccess)
		{
			bPassed &= Require(
				Loaded.RuntimeMap.TypeIdByStableId.contains(EnumId)
					&& Loaded.RuntimeMap.TypeIdByStableId.contains(ValueId)
					&& Loaded.RuntimeMap.TypeIdByStableId.contains(ObjectId)
					&& Loaded.RuntimeMap.FunctionIdByStableId.contains(MethodId)
					&& Loaded.RuntimeMap.FunctionIdByStableId.contains(
						GlobalFunctionId)
					&& Loaded.RuntimeMap.PropertyIndexByStableId.contains(
						ObjectPropertyId)
					&& Loaded.RuntimeMap.PropertyIndexByStableId.contains(
						GlobalPropertyId),
				Name,
				"stable symbol to runtime descriptor map is incomplete");
			const char* Script =
				"int Validate(EMode Mode, FVector V, UObject Obj) { "
				"Obj.Ping(); Obj.Value = GValue; return GetValue(); }\n"
				"int main() { return GetValue(); }\n";
			asIScriptModule* Module =
				Engine->GetModule("ue-validation", asGM_ALWAYS_CREATE);
			const bool bBuilt = Module != nullptr
				&& Module->AddScriptSection("validation.as", Script) >= 0
				&& Module->Build() >= 0;
			bPassed &= Require(
				bBuilt,
				Name,
				"trap-backed imported declarations did not compile");
			if (bBuilt)
			{
				asIScriptContext* Context = Engine->CreateContext();
				asIScriptFunction* Main =
					Module->GetFunctionByDecl("int main()");
				const int Outcome =
					Context != nullptr
						&& Main != nullptr
						&& Context->Prepare(Main) >= 0
					? Context->Execute()
					: asEXECUTION_ERROR;
				bPassed &= Require(
					Outcome == asEXECUTION_EXCEPTION,
					Name,
					"registered UE callable escaped the compile-only trap");
				if (Context != nullptr)
				{
					Context->Release();
				}
			}

			FStandaloneSourceGraphRequest GraphRequest;
			GraphRequest.Entry = "Game/Child.as";
			GraphRequest.Sources = {{
				"Game/Child.as",
				"UCLASS()\n"
				"class UChild : UObject\n"
				"{\n"
				"  UFUNCTION(BlueprintOverride)\n"
				"  void Ping() { Value = 1; }\n"
				"  int ValidateInherited() { Ping(); return Value; }\n"
				"}\n",
			}};
			FStandaloneSourceGraphResult Graph =
				BuildStandaloneSourceGraph(GraphRequest);
			const FBundleTypeOracle Oracle(*Indices);
			const FDeclarationAnalysisResult Analysis =
				Graph.bSuccess
					? AnalyzeStandaloneDeclarations(
						Graph.Modules,
						Oracle)
					: FDeclarationAnalysisResult{};
			bPassed &= Require(
				Graph.bSuccess
					&& Analysis.bSuccess
					&& Analysis.PreClassBindings.size() == 1,
				Name,
				Graph.bSuccess ? Analysis.Error : Graph.Error);
			if (Graph.bSuccess
				&& Analysis.bSuccess
				&& Analysis.PreClassBindings.size() == 1)
			{
				asIScriptModule* ShadowModule =
					Engine->GetModule(
						"source-shadow",
						asGM_ALWAYS_CREATE);
				const auto BaseRuntimeId =
					Loaded.RuntimeMap.TypeIdByStableId.find(ObjectId);
				asPreClassData Data;
				Data.ShadowType = BaseRuntimeId
						!= Loaded.RuntimeMap.TypeIdByStableId.end()
					? Engine->GetTypeInfoById(BaseRuntimeId->second)
					: nullptr;
				if (ShadowModule != nullptr && Data.ShadowType != nullptr)
				{
					ShadowModule->AddPreClassData("UChild", Data);
				}
				const bool bShadowBuilt =
					ShadowModule != nullptr
					&& Data.ShadowType != nullptr
					&& ShadowModule->AddScriptSection(
						Graph.Modules[0].LogicalPath.c_str(),
						Graph.Modules[0].ProcessedSource.c_str(),
						Graph.Modules[0].ProcessedSource.size()) >= 0
					&& ShadowModule->Build() >= 0;
				bPassed &= Require(
					bShadowBuilt,
					Name,
					"compile-only pre-class shadow did not preserve "
						"inherited host member lookup");
			}
		}
		Engine->ShutDownAndRelease();
		asThreadCleanup();
		return bPassed;
	}

	bool TestUECompilerIsDeterministicAndValidationOnly()
	{
		constexpr std::string_view Name =
			"UECompilerIsDeterministicAndValidationOnly";
		const auto Unique = std::chrono::steady_clock::now()
			.time_since_epoch()
			.count();
		const std::filesystem::path Root =
			std::filesystem::temp_directory_path()
			/ ("angelscript-ue-compiler-" + std::to_string(Unique));
		const std::filesystem::path SourceRoot = Root / "source";
		const std::filesystem::path BundleRoot = Root / "bundle";
		std::filesystem::create_directories(SourceRoot);
		std::error_code ErrorCode;
		std::filesystem::copy(
			std::filesystem::path(ANGELSCRIPT_OFFLINE_FIXTURE_ROOT)
				/ "project",
			BundleRoot,
			std::filesystem::copy_options::recursive,
			ErrorCode);
		if (!Require(
				!ErrorCode,
				Name,
				"could not copy frozen producer fixture"))
		{
			return false;
		}
		{
			std::ifstream Input(
				BundleRoot / "manifest.json",
				std::ios::binary);
			std::string Manifest(
				(std::istreambuf_iterator<char>(Input)),
				std::istreambuf_iterator<char>());
			const auto ReplaceOne =
				[&](const std::string_view Before, const std::string_view After)
				{
					const std::size_t Position = Manifest.find(Before);
					if (Position != std::string::npos)
					{
						Manifest.replace(Position, Before.size(), After);
					}
				};
			ReplaceOne(
				"\"compilerContractVersion\":\"test\"",
				"\"compilerContractVersion\":\"ue-as-standalone-v1\"");
			ReplaceOne(
				"\"forkVersion\":\"test\"",
				"\"forkVersion\":\"2.33+selective-2.38\"");
			std::ofstream Output(
				BundleRoot / "manifest.json",
				std::ios::binary | std::ios::trunc);
			Output.write(
				Manifest.data(),
				static_cast<std::streamsize>(Manifest.size()));
		}
		{
			std::ofstream Output(
				SourceRoot / "Main.as",
				std::ios::binary);
			Output
				<< "delegate void FOnValidated(float Value);\n"
				<< "event void FOnBroadcast(int Value);\n"
				<< "UCLASS()\n"
				<< "class ULocal\n"
				<< "{\n"
				<< "  UPROPERTY()\n"
				<< "  int Value;\n"
				<< "  UPROPERTY()\n"
				<< "  FOnBroadcast Broadcast;\n"
				<< "  UFUNCTION()\n"
				<< "  int Validate() { FOnValidated Callback; "
					"Callback.ExecuteIfBound(1.0); "
					"Broadcast.Broadcast(7); return Value + 42; }\n"
				<< "}\n";
		}

		FUECompileRequest Request;
		Request.ScriptRoots = {SourceRoot};
		Request.Entry = "Main.as";
		Request.ExplicitBundle = BundleRoot;
		Request.PackagedDefaultBundle = Root / "not-used";
		Request.bEmitByteCode = true;
		const FUECompiler Compiler;
		const FUECompileResult First = Compiler.Compile(Request);
		const FUECompileResult Second = Compiler.Compile(Request);

		bool bPassed = Require(First.bSuccess, Name, First.Error);
		bPassed &= Require(Second.bSuccess, Name, Second.Error);
		if (First.bSuccess && Second.bSuccess)
		{
			bPassed &= Require(
				!First.ByteCode.empty()
					&& First.ByteCode == Second.ByteCode
					&& First.InputHash == Second.InputHash
					&& First.ProfileHash == Second.ProfileHash
					&& First.BundleIdentity == Second.BundleIdentity,
				Name,
				"identical UE-validation inputs were not deterministic");
			bPassed &= Require(
				First.ProfileHash == FUECompiler::GetProfileHash()
					&& First.ProfileHash != FNativeCompiler::GetProfileHash(),
				Name,
				"native and UE-validation profile identities were mixed");
			bPassed &= Require(
				First.SourceModuleIds.size() == 1
					&& First.SourceModuleIds[0] == First.ModuleId,
				Name,
				"UE-validation result omitted selected source identity");

			const std::filesystem::path Output = Root / "artifacts";
			const FValidationArtifactWriteResult FirstWrite =
				WriteValidationArtifacts({Output, &First});
			const std::string FirstResult =
				ReadFile(Output / "result.json");
			const std::string FirstClasses = ReadFile(
				Output / "modules"
					/ (First.ModuleId + ".classes.jsonl"));
			const FValidationArtifactWriteResult SecondWrite =
				WriteValidationArtifacts({Output, &Second});
			bPassed &= Require(
				FirstWrite.bSuccess && SecondWrite.bSuccess,
				Name,
				FirstWrite.bSuccess
					? SecondWrite.Error
					: FirstWrite.Error);
			bPassed &= Require(
				FirstResult == ReadFile(Output / "result.json")
					&& FirstClasses == ReadFile(
						Output / "modules"
							/ (First.ModuleId + ".classes.jsonl")),
				Name,
				"repeated UE-validation artifact publication was not deterministic");
			bPassed &= Require(
				FirstResult.find("\"ueValidationOnly\": true")
						!= std::string::npos
					&& FirstResult.find("\"profile\": \"ue-validation\"")
						!= std::string::npos
					&& FirstResult.find(
							"\"resourcePolicy\": {\"strict\":false}")
						!= std::string::npos
					&& FirstResult.find(
							"\"resources\": {\"found\":0,\"redirected\":0,"
							"\"missing\":0,\"incompatible\":0,\"unknown\":0}")
						!= std::string::npos
					&& FirstResult.find("pointer") == std::string::npos
					&& FirstResult.find("address") == std::string::npos
					&& FirstResult.find("layout") == std::string::npos,
				Name,
				"validation artifact crossed its execution/layout boundary");
			bPassed &= Require(
				First.Declarations.size() == 6
					&& !First.SemanticObservations.empty()
					&& FirstClasses.find("\"kind\":\"delegate\"")
						!= std::string::npos
					&& FirstClasses.find("\"kind\":\"event\"")
						!= std::string::npos
					&& FirstClasses.find("\"kind\":\"class\"")
						!= std::string::npos
					&& FirstClasses.find("\"kind\":\"property\"")
						!= std::string::npos
					&& FirstClasses.find("\"kind\":\"function\"")
						!= std::string::npos
					&& FirstClasses.find(
							"\"kind\":\"semantic-observation\"")
						!= std::string::npos
					&& FirstClasses.find("\"stableTypeId\":\"builtin:int\"")
						!= std::string::npos
					&& FirstClasses.find("0x")
						== std::string::npos,
				Name,
				"UE-validation artifact omitted semantic/class evidence "
				"or serialized a process address");
		}
		std::filesystem::remove_all(Root, ErrorCode);
		return bPassed;
	}
}

int main()
{
	bool bPassed = true;
	bPassed &= TestSourceClosure();
	bPassed &= TestFilesystemCollection();
	bPassed &= TestBaselineReplacementAndRegistrationPlan();
	bPassed &= TestExportedProjectBaselineIdentityReplacement();
	bPassed &= TestCapabilityPolicyAndCompileOnlyTrap();
	bPassed &= TestAvailabilityPolicyRejectsOnlyUsedUnavailableSymbols();
	bPassed &= TestBundleTypeOracleResolvesStableIdentities();
	bPassed &= TestRegistrationLoaderCompilesAndMapsStableSymbols();
	bPassed &= TestUECompilerIsDeterministicAndValidationOnly();
	return bPassed ? 0 : 1;
}
