#include "Compiler/AngelscriptStandaloneUECompiler.h"

#include "Adapters/AngelscriptAdapterRegistry.h"
#include "Compiler/AngelscriptStandaloneDeclarationAnalysis.h"
#include "Compiler/AngelscriptStandaloneSourceGraph.h"
#include "Contract/AngelscriptBundleTypeOracle.h"
#include "Registration/AngelscriptRegistrationLoader.h"
#include "Registration/AngelscriptRegistrationPlan.h"
#include "Registration/AngelscriptScriptBaselinePlan.h"
#include "Registration/AngelscriptCompileOnlyStub.h"
#include "Resources/AngelscriptAssetIndex.h"
#include "Resources/AngelscriptResourceContext.h"
#include "Resources/AngelscriptResourceDiagnostics.h"
#include "Support/AngelscriptStandaloneByteCodeStream.h"
#include "Support/AngelscriptStandaloneHash.h"

#include "UnrealAngelscriptVersion.h"
#include "angelscript.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string_view>

namespace AngelscriptStandalone
{
	namespace
	{
		struct FEngineReleaser
		{
			void operator()(asIScriptEngine* Engine) const
			{
				if (Engine != nullptr)
				{
					Engine->ShutDownAndRelease();
				}
			}
		};

		struct FThreadCleanup
		{
			~FThreadCleanup()
			{
				asThreadCleanup();
			}
		};

		struct FSemanticObserverScope
		{
			FSemanticObserverScope(
				asIScriptEngine& InEngine,
				asISemanticObserver& Observer)
				: Engine(InEngine)
				, Previous(InEngine.SetUserData(
					&Observer,
					asSEMANTIC_OBSERVER_USER_DATA_ID))
			{
			}

			~FSemanticObserverScope()
			{
				Engine.SetUserData(
					Previous,
					asSEMANTIC_OBSERVER_USER_DATA_ID);
			}

			asIScriptEngine& Engine;
			void* Previous = nullptr;
		};

		EDiagnosticSeverity ConvertSeverity(
			const Frontend::EDiagnosticSeverity Severity)
		{
			switch (Severity)
			{
			case Frontend::EDiagnosticSeverity::Info:
				return EDiagnosticSeverity::Info;
			case Frontend::EDiagnosticSeverity::Warning:
				return EDiagnosticSeverity::Warning;
			case Frontend::EDiagnosticSeverity::Error:
			default:
				return EDiagnosticSeverity::Error;
			}
		}

		void AppendLanguageDiagnostics(
			const std::vector<AngelscriptStandalone::Frontend::FDiagnostic>& Input,
			std::vector<FDiagnostic>& Output)
		{
			for (const auto& Source : Input)
			{
				FDiagnostic& Diagnostic = Output.emplace_back();
				Diagnostic.Code = Source.Code;
				Diagnostic.Message = Source.Message;
				Diagnostic.Section = Source.LogicalPath;
				Diagnostic.Severity = ConvertSeverity(Source.Severity);
				Diagnostic.Row = 1;
				Diagnostic.Column =
					static_cast<int>(Source.Span.Begin + 1);
			}
		}

		void AppendEngineDiagnostics(
			const FDiagnosticSink& Sink,
			std::vector<FDiagnostic>& Output)
		{
			const auto& Diagnostics = Sink.GetDiagnostics();
			Output.insert(
				Output.end(),
				Diagnostics.begin(),
				Diagnostics.end());
		}

		void TraceCompilePhase(const std::string_view Phase)
		{
			if (std::getenv("AS_STANDALONE_TRACE") != nullptr)
			{
				std::cerr << "as-standalone trace: "
					<< Phase << '\n';
			}
		}

		std::string SourceFuncdefSignature(
			const AngelscriptStandalone::Frontend::FDeclaration& Declaration)
		{
			std::string_view Signature = Declaration.Declaration;
			if (Signature.starts_with("delegate"))
			{
				Signature.remove_prefix(8);
			}
			else if (Signature.starts_with("event"))
			{
				Signature.remove_prefix(5);
			}
			while (!Signature.empty()
				&& (Signature.front() == ' '
					|| Signature.front() == '\t'
					|| Signature.front() == '\r'
					|| Signature.front() == '\n'))
			{
				Signature.remove_prefix(1);
			}
			return std::string(Signature);
		}

		bool RegisterSourceDelegateProxy(
			asIScriptEngine& Engine,
			const AngelscriptStandalone::Frontend::FDeclaration& Declaration,
			const FOfflineManifest& Manifest,
			int& OutTypeId,
			std::string& OutError)
		{
			const std::string Signature =
				SourceFuncdefSignature(Declaration);
			const std::size_t Open = Signature.find('(');
			const std::size_t Close = Signature.rfind(')');
			if (Open == std::string::npos
				|| Close == std::string::npos
				|| Close < Open)
			{
				OutError =
					"source delegate/event has no parameter clause: "
					+ Declaration.QualifiedName;
				return false;
			}
			const std::string Parameters =
				Signature.substr(Open, Close - Open + 1);
			const std::string TypeName = Declaration.Name;
			OutTypeId = Engine.RegisterObjectType(
				TypeName.c_str(),
				16,
				asOBJ_VALUE | asOBJ_APP_CLASS_ALLINTS);
			if (OutTypeId < 0)
			{
				OutError =
					"failed to register compile-only source delegate/event type: "
					+ Declaration.QualifiedName;
				return false;
			}

			auto RegisterBehaviour =
				[&](const asEBehaviours Behaviour,
					const std::string& Method) -> bool
				{
					return Engine.RegisterObjectBehaviour(
						TypeName.c_str(),
						Behaviour,
						Method.c_str(),
						asFUNCTION(CompileOnlyTrap),
						asCALL_GENERIC) >= 0;
				};
			auto RegisterMethod = [&](const std::string& Method) -> bool
				{
					const std::string Normalized =
						NormalizeApplicationRegistrationDeclaration(
							Method,
							Manifest);
					return Engine.RegisterObjectMethod(
						TypeName.c_str(),
						Normalized.c_str(),
						asFUNCTION(CompileOnlyTrap),
						asCALL_GENERIC) >= 0;
				};

			if (!RegisterBehaviour(asBEHAVE_CONSTRUCT, "void f()")
				|| !RegisterBehaviour(
					asBEHAVE_CONSTRUCT,
					"void f(const " + TypeName + "&in Other)")
				|| !RegisterBehaviour(asBEHAVE_DESTRUCT, "void f()")
				|| !RegisterMethod(
					TypeName + "& opAssign(const "
						+ TypeName + "&in Other)")
				|| !RegisterMethod("bool IsBound() const"))
			{
				OutError =
					"failed to register compile-only source delegate/event "
					"lifetime surface: " + Declaration.QualifiedName;
				return false;
			}

			const bool bEvent =
				Declaration.Declaration.starts_with("event ");
			const std::string ReturnType =
				Declaration.Type.Spelling.empty()
					? "void"
					: Declaration.Type.Spelling;
			if (bEvent)
			{
				if (!RegisterMethod(
						ReturnType + " Broadcast" + Parameters
							+ " const"))
				{
					OutError =
						"failed to register source event broadcast surface: "
						+ Declaration.QualifiedName;
					return false;
				}
			}
			else if (!RegisterMethod(
					ReturnType + " Execute" + Parameters + " const")
				|| !RegisterMethod(
					ReturnType + " ExecuteIfBound" + Parameters
						+ " const"))
			{
				OutError =
					"failed to register source delegate execution surface: "
					+ Declaration.QualifiedName;
				return false;
			}

			if (Engine.GetTypeIdByDecl("FName") >= 0)
			{
				const std::string BindingMethod = bEvent
					? "void AddUFunction(const UObject Object, "
						"FName FunctionName)"
					: "void BindUFunction(UObject Object, "
						"FName FunctionName)";
				if (!RegisterMethod(BindingMethod))
				{
					OutError =
						"failed to register source delegate/event binding "
						"surface: " + Declaration.QualifiedName;
					return false;
				}
			}
			return true;
		}

		AngelscriptStandalone::Frontend::FPreprocessConfig MakePreprocessConfig(
			const FOfflineManifest& Manifest)
		{
			AngelscriptStandalone::Frontend::FPreprocessConfig Config;
			Config.Flags.emplace("EDITOR", false);
			Config.Flags.emplace("EDITORONLY_DATA", false);
			Config.Flags.emplace("COOK_COMMANDLET", false);
			Config.Flags.emplace("RELEASE", false);
			Config.Flags.emplace("TEST", true);
			Config.Flags.emplace("WITH_SERVER_CODE", false);
			for (const auto& [Name, Value] : Manifest.FeatureFlags)
			{
				Config.Flags.insert_or_assign(Name, Value);
			}
			return Config;
		}
	}

	std::string FUECompiler::GetProfileHash()
	{
		const std::string Profile =
			std::string("profile=ue-validation-v1\n")
			+ "product=" UNREAL_ANGELSCRIPT_VERSION_STRING "\n"
			+ "upstream=" UNREAL_ANGELSCRIPT_UPSTREAM_LINEAGE_STRING "\n"
			+ "fork=2.33+selective-2.38\n"
			+ "compiler-contract=ue-as-standalone-v1\n"
			+ "execution=forbidden\n";
		return Sha256(Profile);
	}

	FUECompileResult FUECompiler::Compile(
		const FUECompileRequest& Request) const
	{
		FUECompileResult Result;
		Result.ProfileHash = GetProfileHash();
		Result.bStrictResources = Request.bStrictResources;

		FOfflineBundleLoadOptions BundleOptions;
		BundleOptions.Compatibility.ExpectedForkVersion =
			"2.33+selective-2.38";
		BundleOptions.Compatibility.ExpectedCompilerContractVersion =
			"ue-as-standalone-v1";
		const FOfflineBundleLoadResult Bundle = LoadSelectedOfflineBundle(
			Request.ExplicitBundle,
			Request.PackagedDefaultBundle,
			BundleOptions);
		TraceCompilePhase("bundle-loaded");
		if (!Bundle.bSuccess)
		{
			Result.bInfrastructureFailure = true;
			Result.Error = Bundle.Error;
			return Result;
		}
		Result.BundleIdentity = Bundle.Bundle.Manifest.BundleIdentity;
		Result.BundleDirectory = Bundle.Bundle.Directory;
		Result.BundleKind = Bundle.Bundle.Manifest.BundleKind;
		Result.SymbolScope = Bundle.Bundle.Manifest.SymbolScope;
		Result.AssetScope = Bundle.Bundle.Manifest.AssetScope;
		const FAdapterHandshakeResult AdapterHandshake =
			ValidateAdapterHandshake(Bundle.Bundle.Manifest);
		if (!AdapterHandshake.bSuccess)
		{
			Result.bInfrastructureFailure = true;
			Result.Error = AdapterHandshake.Error;
			return Result;
		}

		const FStandaloneSourceCollectionResult Sources =
			CollectStandaloneSources(Request.ScriptRoots);
		if (!Sources.bSuccess)
		{
			Result.bInfrastructureFailure = true;
			Result.Error = Sources.Error;
			return Result;
		}
		FStandaloneSourceGraphRequest GraphRequest;
		GraphRequest.Sources = Sources.Sources;
		GraphRequest.Entry = Request.Entry.generic_string();
		GraphRequest.Config = MakePreprocessConfig(Bundle.Bundle.Manifest);
		FStandaloneSourceGraphResult Graph =
			BuildStandaloneSourceGraph(GraphRequest);
		if (!Graph.bSuccess)
		{
			Result.Error = Graph.Error;
			AppendLanguageDiagnostics(Graph.Diagnostics, Result.Diagnostics);
			return Result;
		}
		const FBundleTypeOracle TypeOracle(*Bundle.Bundle.Indices);
		const FDeclarationAnalysisResult DeclarationAnalysis =
			AnalyzeStandaloneDeclarations(Graph.Modules, TypeOracle);
		TraceCompilePhase("declarations-analyzed");
		Result.Declarations = DeclarationAnalysis.Declarations;
		Result.Capabilities = DeclarationAnalysis.Capabilities;
		if (!DeclarationAnalysis.bSuccess)
		{
			Result.Error = DeclarationAnalysis.Error;
			AppendLanguageDiagnostics(
				DeclarationAnalysis.Diagnostics,
				Result.Diagnostics);
			return Result;
		}
		Result.LogicalEntryPath =
			Graph.Modules.empty() ? std::string() : Graph.Modules.back().LogicalPath;
		Result.ModuleId =
			Graph.Modules.empty() ? std::string() : Graph.Modules.back().ModuleId;
		std::string InputIdentity = "ue-input-v1\n";
		for (const auto& Module : Graph.Modules)
		{
			Result.SourceModuleIds.push_back(Module.ModuleId);
			InputIdentity += Module.ModuleId + "\n"
				+ Module.LogicalPath + "\n"
				+ Module.ProcessedSource + "\n";
		}
		Result.InputHash = Sha256(InputIdentity);

		std::string AssetIndexError;
		const std::shared_ptr<const FAssetIndex> AssetIndex =
			FAssetIndex::Build(
				Bundle.Bundle.Indices->Assets(),
				Bundle.Bundle.Manifest.AssetScope,
				AssetIndexError);
		if (AssetIndex == nullptr)
		{
			Result.bInfrastructureFailure = true;
			Result.Error = AssetIndexError;
			return Result;
		}
		const FScriptBaselinePlan Baseline =
			BuildScriptBaselinePlan(
				*Bundle.Bundle.Indices,
				Graph.Modules);
		if (!Baseline.bSuccess)
		{
			Result.bInfrastructureFailure = true;
			Result.Error = Baseline.Error;
			return Result;
		}
		Result.ReplacedBaselineModuleIds = Baseline.ReplacedModuleIds;
		const FRegistrationPlan Plan = BuildRegistrationPlan(
			Bundle.Bundle.Manifest,
			Baseline,
			Graph.Modules);
		if (!Plan.bSuccess)
		{
			Result.bInfrastructureFailure = true;
			Result.Error = Plan.Error;
			return Result;
		}

		FDiagnosticSink DiagnosticSink;
		FThreadCleanup ThreadCleanup;
		FRegistrationLoadResult Registrations;
		std::unique_ptr<asIScriptEngine, FEngineReleaser> Engine(
			asCreateScriptEngine());
		if (!Engine)
		{
			Result.bInfrastructureFailure = true;
			Result.Error = "asCreateScriptEngine returned null";
			return Result;
		}
		Engine->SetMessageCallback(
			asFUNCTION(FDiagnosticSink::MessageCallback),
			&DiagnosticSink,
			asCALL_CDECL);
		Registrations = ApplyRegistrationPlan(
			Engine.get(),
			Plan,
			Bundle.Bundle.Manifest);
		TraceCompilePhase("bundle-registered");
		Result.Capabilities.insert(
			Result.Capabilities.end(),
			Registrations.Capabilities.begin(),
			Registrations.Capabilities.end());
		if (!Registrations.bSuccess)
		{
			Result.bInfrastructureFailure = true;
			Result.Error = Registrations.Error;
			AppendEngineDiagnostics(DiagnosticSink, Result.Diagnostics);
			return Result;
		}
		for (const auto& Declaration : Result.Declarations)
		{
			const bool bSourceDelegate =
				Declaration.Declaration.starts_with("delegate ");
			const bool bSourceEvent =
				Declaration.Declaration.starts_with("event ");
			if (!bSourceDelegate && !bSourceEvent)
			{
				continue;
			}
			const std::string PreviousNamespace =
				Engine->GetDefaultNamespace();
			if (Engine->SetDefaultNamespace(
					Declaration.Namespace.c_str()) < 0)
			{
				Result.bInfrastructureFailure = true;
				Result.Error =
					"failed to select source delegate namespace: "
					+ Declaration.Namespace;
				return Result;
			}
			int TypeId = -1;
			std::string ProxyError;
			const bool bRegistered = RegisterSourceDelegateProxy(
				*Engine,
				Declaration,
				Bundle.Bundle.Manifest,
				TypeId,
				ProxyError);
			Engine->SetDefaultNamespace(PreviousNamespace.c_str());
			if (!bRegistered)
			{
				Result.Error =
					std::move(ProxyError);
				AppendEngineDiagnostics(
					DiagnosticSink,
					Result.Diagnostics);
				return Result;
			}
			Registrations.RuntimeMap.TypeIdByStableId.insert_or_assign(
				Declaration.StableId,
				TypeId);
		}
		Result.CapabilitySummary = SummarizeCapabilities(
			Result.Capabilities,
			Request.bAllowUERequired);
		if (!Result.CapabilitySummary.bCanCompile)
		{
			Result.Error =
				"UE validation contains disallowed capability classifications";
			return Result;
		}

		asIScriptModule* Module =
			Engine->GetModule(Result.ModuleId.c_str(), asGM_ALWAYS_CREATE);
		if (Module == nullptr)
		{
			Result.bInfrastructureFailure = true;
			Result.Error = "failed to create UE-validation script module";
			return Result;
		}
		FStandaloneSemanticObserver SemanticObserver(
			*Engine,
			Registrations.RuntimeMap);
		FSemanticObserverScope SemanticObserverScope(
			*Engine,
			SemanticObserver);
		for (const FPreClassBinding& Binding
			: DeclarationAnalysis.PreClassBindings)
		{
			const auto RuntimeType =
				Registrations.RuntimeMap.TypeIdByStableId.find(
					Binding.BaseStableTypeId);
			if (RuntimeType
				== Registrations.RuntimeMap.TypeIdByStableId.end())
			{
				Result.bInfrastructureFailure = true;
				Result.Error =
					"pre-class host base was not registered: "
					+ Binding.BaseStableTypeId;
				return Result;
			}
			asITypeInfo* ShadowType =
				Engine->GetTypeInfoById(RuntimeType->second);
			if (ShadowType == nullptr)
			{
				Result.bInfrastructureFailure = true;
				Result.Error =
					"pre-class host base runtime descriptor is missing: "
					+ Binding.BaseStableTypeId;
				return Result;
			}
			asPreClassData Data;
			Data.ShadowType = ShadowType;
			Module->AddPreClassData(
				Binding.ClassName.c_str(),
				Data);
		}
		for (const auto& Source : Graph.Modules)
		{
			if (Module->AddScriptSection(
					Source.LogicalPath.c_str(),
					Source.ProcessedSource.data(),
					Source.ProcessedSource.size()) < 0)
			{
				Result.bInfrastructureFailure = true;
				Result.Error = "failed to add a UE-validation source section";
				AppendEngineDiagnostics(
					DiagnosticSink,
					Result.Diagnostics);
				return Result;
			}
		}
		TraceCompilePhase("module-build-start");
		if (Module->Build() < 0)
		{
			Result.Error = "UE-validation source compilation failed";
			AppendEngineDiagnostics(DiagnosticSink, Result.Diagnostics);
			return Result;
		}
		TraceCompilePhase("module-build-finished");
		Result.SemanticObservations =
			SemanticObserver.GetObservations();
		AppendUsedAvailabilityCapabilities(
			Result.SemanticObservations,
			Registrations.RuntimeMap.AvailabilityByStableId,
			Result.Capabilities);

		const FResourceContextResult ResourceContexts =
			DiscoverResourceContexts(
				Graph.Modules,
				Result.Declarations,
				*Bundle.Bundle.Indices,
				&Result.SemanticObservations);
		if (!ResourceContexts.bSuccess)
		{
			Result.bInfrastructureFailure = true;
			Result.Error = ResourceContexts.Error;
			return Result;
		}
		bool bResourceFailure = false;
		for (const FResourceContext& Context
			: ResourceContexts.Contexts)
		{
			FResourceValidation Validation =
				ValidateResourceContext(
					Context,
					*AssetIndex,
					*Bundle.Bundle.Indices);
			bResourceFailure |= IsResourceFailure(
				Validation,
				Request.bStrictResources);
			if (Validation.State != EResourceState::Found)
			{
				Result.Diagnostics.push_back(
					MakeResourceDiagnostic(
						Validation,
						Request.bStrictResources));
			}
			if (Validation.State == EResourceState::Unknown
				&& !Context.bSoft)
			{
				Result.Capabilities.push_back({
					Context.ContextId,
					"resource-scope-unknown",
					ECapabilityClassification::UERequired,
					"hard resource resolution cannot be decided by "
						"the selected offline asset scope",
				});
			}
			Result.Resources.push_back(std::move(Validation));
		}
		Result.CapabilitySummary = SummarizeCapabilities(
			Result.Capabilities,
			Request.bAllowUERequired);
		if (Request.bEmitByteCode)
		{
			FMemoryByteCodeStream Stream;
			if (Module->SaveByteCode(&Stream) < 0)
			{
				Result.Error = "UE-validation bytecode generation failed";
				AppendEngineDiagnostics(
					DiagnosticSink,
					Result.Diagnostics);
				return Result;
			}
			Result.ByteCode = std::move(Stream.Buffer);
		}
		AppendEngineDiagnostics(DiagnosticSink, Result.Diagnostics);
		Result.bSuccess = !bResourceFailure;
		if (bResourceFailure)
		{
			Result.Error =
				"UE-validation resource policy rejected one or more "
				"typed resource contexts";
		}
		return Result;
	}
}
