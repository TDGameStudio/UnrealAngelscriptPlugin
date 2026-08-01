#include "Compiler/AngelscriptValidationArtifact.h"

#include "Support/AngelscriptStandaloneHash.h"
#include "Support/AngelscriptStandaloneJson.h"

#include "UnrealAngelscriptVersion.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace AngelscriptStandalone
{
	namespace
	{
		bool WriteText(
			const std::filesystem::path& Path,
			const std::string_view Text)
		{
			std::ofstream Output(
				Path,
				std::ios::binary | std::ios::trunc);
			Output.write(
				Text.data(),
				static_cast<std::streamsize>(Text.size()));
			return Output.good();
		}

		bool WriteBinary(
			const std::filesystem::path& Path,
			const std::vector<std::uint8_t>& Bytes)
		{
			std::ofstream Output(
				Path,
				std::ios::binary | std::ios::trunc);
			Output.write(
				reinterpret_cast<const char*>(Bytes.data()),
				static_cast<std::streamsize>(Bytes.size()));
			return Output.good();
		}

		const char* BundleKindName(const EOfflineBundleKind Kind)
		{
			return Kind == EOfflineBundleKind::Project
				? "project"
				: "default-engine";
		}

		std::string ScopeJson(const FOfflineScope& Scope)
		{
			std::ostringstream Json;
			Json << "{\"complete\":"
				<< (Scope.bComplete ? "true" : "false")
				<< ",\"state\":" << EscapeJsonString(Scope.State)
				<< "}";
			return Json.str();
		}

		std::string DiagnosticsJsonLines(
			const std::vector<FDiagnostic>& Diagnostics)
		{
			std::ostringstream Lines;
			for (const FDiagnostic& Diagnostic : Diagnostics)
			{
				Lines
					<< "{\"code\":" << EscapeJsonString(Diagnostic.Code)
					<< ",\"column\":" << Diagnostic.Column
					<< ",\"message\":" << EscapeJsonString(Diagnostic.Message)
					<< ",\"row\":" << Diagnostic.Row
					<< ",\"section\":" << EscapeJsonString(Diagnostic.Section)
					<< ",\"severity\":"
					<< EscapeJsonString(ToString(Diagnostic.Severity))
					<< ",\"evidence\":{";
				bool bFirstEvidence = true;
				for (const auto& [Name, Value] : Diagnostic.Evidence)
				{
					if (!bFirstEvidence)
					{
						Lines << ',';
					}
					bFirstEvidence = false;
					Lines
						<< EscapeJsonString(Name) << ':'
						<< EscapeJsonString(Value);
				}
				Lines << "}}\n";
			}
			return Lines.str();
		}

		const char* DeclarationKindName(
			const AngelscriptStandalone::Frontend::EDeclarationKind Kind)
		{
			using AngelscriptStandalone::Frontend::EDeclarationKind;
			switch (Kind)
			{
			case EDeclarationKind::Class:
				return "class";
			case EDeclarationKind::Struct:
				return "struct";
			case EDeclarationKind::Enum:
				return "enum";
			case EDeclarationKind::Delegate:
				return "delegate";
			case EDeclarationKind::Property:
				return "property";
			case EDeclarationKind::Function:
				return "function";
			case EDeclarationKind::Event:
				return "event";
			case EDeclarationKind::Global:
			default:
				return "global";
			}
		}

		const char* AccessName(
			const AngelscriptStandalone::Frontend::EAccess Access)
		{
			using AngelscriptStandalone::Frontend::EAccess;
			switch (Access)
			{
			case EAccess::Public:
				return "public";
			case EAccess::Protected:
				return "protected";
			case EAccess::Private:
				return "private";
			case EAccess::Unspecified:
			default:
				return "unspecified";
			}
		}

		std::string TypeReferenceJson(
			const AngelscriptStandalone::Frontend::FTypeReference& Type)
		{
			std::ostringstream Json;
			Json
				<< "{\"const\":" << (Type.bConst ? "true" : "false")
				<< ",\"handle\":" << (Type.bHandle ? "true" : "false")
				<< ",\"reference\":"
				<< (Type.bReference ? "true" : "false")
				<< ",\"spelling\":" << EscapeJsonString(Type.Spelling)
				<< ",\"stableTypeId\":"
				<< EscapeJsonString(Type.StableTypeId)
				<< '}';
			return Json.str();
		}

		std::string DeclarationJson(
			const AngelscriptStandalone::Frontend::FDeclaration& Declaration,
			const std::vector<std::string>& ResourceDiagnosticIds)
		{
			std::ostringstream Json;
			Json
				<< "{\"access\":"
				<< EscapeJsonString(AccessName(Declaration.Access))
				<< ",\"bases\":[";
			for (std::size_t Index = 0;
				Index < Declaration.BaseTypes.size();
				++Index)
			{
				if (Index != 0)
				{
					Json << ',';
				}
				Json << TypeReferenceJson(Declaration.BaseTypes[Index]);
			}
			Json
				<< "],\"const\":"
				<< (Declaration.bConst ? "true" : "false")
				<< ",\"declaration\":"
				<< EscapeJsonString(Declaration.Declaration)
				<< ",\"default\":"
				<< EscapeJsonString(Declaration.DefaultValue)
				<< ",\"enumValues\":[";
			for (std::size_t Index = 0;
				Index < Declaration.EnumValues.size();
				++Index)
			{
				if (Index != 0)
				{
					Json << ',';
				}
				Json
					<< "{\"name\":"
					<< EscapeJsonString(
						Declaration.EnumValues[Index].Name)
					<< ",\"value\":"
					<< EscapeJsonString(
						Declaration.EnumValues[Index].ValueExpression)
					<< '}';
			}
			Json
				<< ']'
				<< ",\"final\":"
				<< (Declaration.bFinal ? "true" : "false")
				<< ",\"kind\":"
				<< EscapeJsonString(DeclarationKindName(Declaration.Kind))
				<< ",\"metadata\":[";
			for (std::size_t Index = 0;
				Index < Declaration.Metadata.size();
				++Index)
			{
				if (Index != 0)
				{
					Json << ',';
				}
				Json
					<< "{\"name\":"
					<< EscapeJsonString(Declaration.Metadata[Index].Name)
					<< ",\"value\":"
					<< EscapeJsonString(Declaration.Metadata[Index].Value)
					<< '}';
			}
			Json
				<< "],\"moduleId\":"
				<< EscapeJsonString(Declaration.ModuleId)
				<< ",\"name\":" << EscapeJsonString(Declaration.Name)
				<< ",\"namespace\":"
				<< EscapeJsonString(Declaration.Namespace)
				<< ",\"override\":"
				<< (Declaration.bOverride ? "true" : "false")
				<< ",\"owner\":" << EscapeJsonString(Declaration.Owner)
				<< ",\"parameters\":[";
			for (std::size_t Index = 0;
				Index < Declaration.Parameters.size();
				++Index)
			{
				if (Index != 0)
				{
					Json << ',';
				}
				const auto& Parameter = Declaration.Parameters[Index];
				Json
					<< "{\"default\":"
					<< EscapeJsonString(Parameter.DefaultValue)
					<< ",\"name\":" << EscapeJsonString(Parameter.Name)
					<< ",\"type\":"
					<< TypeReferenceJson(Parameter.Type)
					<< '}';
			}
			Json
				<< "],\"qualifiedName\":"
				<< EscapeJsonString(Declaration.QualifiedName)
				<< ",\"resourceDiagnosticIds\":[";
			for (std::size_t Index = 0;
				Index < ResourceDiagnosticIds.size();
				++Index)
			{
				if (Index != 0)
				{
					Json << ',';
				}
				Json << EscapeJsonString(ResourceDiagnosticIds[Index]);
			}
			Json
				<< ']'
				<< ",\"source\":{\"begin\":"
				<< Declaration.Source.Begin
				<< ",\"end\":" << Declaration.Source.End
				<< ",\"logicalPath\":"
				<< EscapeJsonString(Declaration.Source.LogicalPath)
				<< "},\"stableId\":"
				<< EscapeJsonString(Declaration.StableId)
				<< ",\"static\":"
				<< (Declaration.bStatic ? "true" : "false")
				<< ",\"type\":" << TypeReferenceJson(Declaration.Type)
				<< "}\n";
			return Json.str();
		}

		std::string ClassesJsonLines(const FUECompileResult& Result)
		{
			std::vector<AngelscriptStandalone::Frontend::FDeclaration> Declarations =
				Result.Declarations;
			std::sort(
				Declarations.begin(),
				Declarations.end(),
				[](const auto& Left, const auto& Right)
				{
					return Left.StableId < Right.StableId;
				});
			std::vector<FCapabilityObservation> Capabilities =
				Result.Capabilities;
			std::sort(
				Capabilities.begin(),
				Capabilities.end(),
				[](const auto& Left, const auto& Right)
				{
					if (Left.StableId != Right.StableId)
					{
						return Left.StableId < Right.StableId;
					}
					return Left.Rule < Right.Rule;
				});
			std::ostringstream Lines;
			for (const auto& Declaration : Declarations)
			{
				std::vector<std::string> ResourceDiagnosticIds;
				for (const FResourceValidation& Resource : Result.Resources)
				{
					if (Resource.Context.ContextStableSymbolId
							== Declaration.StableId
						&& !Resource.DiagnosticId.empty())
					{
						ResourceDiagnosticIds.push_back(
							Resource.DiagnosticId);
					}
				}
				std::sort(
					ResourceDiagnosticIds.begin(),
					ResourceDiagnosticIds.end());
				Lines << DeclarationJson(
					Declaration,
					ResourceDiagnosticIds);
			}
			std::vector<FSemanticObservation> SemanticObservations =
				Result.SemanticObservations;
			std::sort(
				SemanticObservations.begin(),
				SemanticObservations.end(),
				[](const auto& Left, const auto& Right)
				{
					if (Left.LogicalPath != Right.LogicalPath)
						return Left.LogicalPath < Right.LogicalPath;
					if (Left.Source.Begin != Right.Source.Begin)
						return Left.Source.Begin < Right.Source.Begin;
					if (Left.Kind != Right.Kind)
					{
						return static_cast<int>(Left.Kind)
							< static_cast<int>(Right.Kind);
					}
					if (Left.StableFunctionId
						!= Right.StableFunctionId)
					{
						return Left.StableFunctionId
							< Right.StableFunctionId;
					}
					return Left.TargetStableTypeId
						< Right.TargetStableTypeId;
				});
			for (const FSemanticObservation& Observation
				: SemanticObservations)
			{
				Lines
					<< "{\"arguments\":[";
				for (std::size_t Index = 0;
					Index < Observation.Arguments.size();
					++Index)
				{
					if (Index != 0)
						Lines << ',';
					const FSemanticArgumentObservation& Argument =
						Observation.Arguments[Index];
					Lines
						<< "{\"actualStableTypeId\":"
						<< EscapeJsonString(
							Argument.ActualStableTypeId)
						<< ",\"actualTypeDeclaration\":"
						<< EscapeJsonString(
							Argument.ActualTypeDeclaration)
						<< ",\"parameterStableTypeId\":"
						<< EscapeJsonString(
							Argument.ParameterStableTypeId)
						<< ",\"parameterTypeDeclaration\":"
						<< EscapeJsonString(
							Argument.ParameterTypeDeclaration)
						<< ",\"source\":{\"begin\":"
						<< Argument.Source.Begin
						<< ",\"end\":" << Argument.Source.End
						<< "}}";
				}
				Lines
					<< "],\"column\":" << Observation.Column
					<< ",\"constantString\":"
					<< EscapeJsonString(
						Observation.ConstantString)
					<< ",\"kind\":\"semantic-observation\""
					<< ",\"logicalPath\":"
					<< EscapeJsonString(
						Observation.LogicalPath)
					<< ",\"observation\":"
					<< EscapeJsonString(
						ToString(Observation.Kind))
					<< ",\"row\":" << Observation.Row
					<< ",\"source\":{\"begin\":"
					<< Observation.Source.Begin
					<< ",\"end\":" << Observation.Source.End
					<< "},\"sourceStableTypeId\":"
					<< EscapeJsonString(
						Observation.SourceStableTypeId)
					<< ",\"sourceTypeDeclaration\":"
					<< EscapeJsonString(
						Observation.SourceTypeDeclaration)
					<< ",\"stableFunctionId\":"
					<< EscapeJsonString(
						Observation.StableFunctionId)
					<< ",\"targetStableTypeId\":"
					<< EscapeJsonString(
						Observation.TargetStableTypeId)
					<< ",\"targetTypeDeclaration\":"
					<< EscapeJsonString(
						Observation.TargetTypeDeclaration)
					<< "}\n";
			}
			for (const FCapabilityObservation& Capability : Capabilities)
			{
				Lines
					<< "{\"classification\":"
					<< EscapeJsonString(ToString(Capability.Classification))
					<< ",\"kind\":\"capability\",\"reason\":"
					<< EscapeJsonString(Capability.Reason)
					<< ",\"rule\":" << EscapeJsonString(Capability.Rule)
					<< ",\"stableId\":"
					<< EscapeJsonString(Capability.StableId)
					<< "}\n";
			}
			return Lines.str();
		}

		std::string ResultJson(const FUECompileResult& Result)
		{
			std::size_t FoundResources = 0;
			std::size_t RedirectedResources = 0;
			std::size_t MissingResources = 0;
			std::size_t IncompatibleResources = 0;
			std::size_t UnknownResources = 0;
			for (const FResourceValidation& Resource : Result.Resources)
			{
				switch (Resource.State)
				{
				case EResourceState::Found:
					++FoundResources;
					break;
				case EResourceState::Redirected:
					++RedirectedResources;
					break;
				case EResourceState::Missing:
					++MissingResources;
					break;
				case EResourceState::Incompatible:
					++IncompatibleResources;
					break;
				case EResourceState::Unknown:
				default:
					++UnknownResources;
					break;
				}
			}
			std::ostringstream Json;
			Json
				<< "{\n"
				<< "  \"assetScope\": " << ScopeJson(Result.AssetScope) << ",\n"
				<< "  \"bundle\": {\"identity\":"
				<< EscapeJsonString(Result.BundleIdentity)
				<< ",\"kind\":" << EscapeJsonString(
					BundleKindName(Result.BundleKind))
				<< ",\"path\":"
				<< EscapeJsonString(
					Result.BundleDirectory.lexically_normal().generic_string())
				<< "},\n"
				<< "  \"capabilities\": {\"compileShim\":"
				<< Result.CapabilitySummary.CompileShimCount
				<< ",\"exact\":" << Result.CapabilitySummary.ExactCount
				<< ",\"ueRequired\":"
				<< Result.CapabilitySummary.UERequiredCount
				<< ",\"unsupported\":"
				<< Result.CapabilitySummary.UnsupportedCount
				<< "},\n"
				<< "  \"compiler\": "
				<< EscapeJsonString(
					UNREAL_ANGELSCRIPT_PRODUCT_VERSION_STRING)
				<< ",\n"
				<< "  \"inputHash\": "
				<< EscapeJsonString(Result.InputHash) << ",\n"
				<< "  \"logicalEntry\": "
				<< EscapeJsonString(Result.LogicalEntryPath) << ",\n"
				<< "  \"module\": {\"bytecodeHash\":";
			if (Result.ByteCode.empty())
			{
				Json << "null,\"bytecodePath\":null";
			}
			else
			{
				Json << EscapeJsonString(Sha256(Result.ByteCode))
					<< ",\"bytecodePath\":"
					<< EscapeJsonString(
						"modules/" + Result.ModuleId + ".asbc");
			}
			Json
				<< ",\"classesPath\":"
				<< EscapeJsonString(
					"modules/" + Result.ModuleId + ".classes.jsonl")
				<< ",\"id\":" << EscapeJsonString(Result.ModuleId)
				<< "},\n"
				<< "  \"profile\": \"ue-validation\",\n"
				<< "  \"profileHash\": "
				<< EscapeJsonString(Result.ProfileHash) << ",\n"
				<< "  \"resourcePolicy\": {\"strict\":"
				<< (Result.bStrictResources ? "true" : "false")
				<< "},\n"
				<< "  \"resources\": {\"found\":" << FoundResources
				<< ",\"redirected\":" << RedirectedResources
				<< ",\"missing\":" << MissingResources
				<< ",\"incompatible\":" << IncompatibleResources
				<< ",\"unknown\":" << UnknownResources << "},\n"
				<< "  \"replacedBaselineModules\": [";
			for (std::size_t Index = 0;
				Index < Result.ReplacedBaselineModuleIds.size();
				++Index)
			{
				if (Index != 0)
				{
					Json << ',';
				}
				Json << EscapeJsonString(
					Result.ReplacedBaselineModuleIds[Index]);
			}
			Json
				<< "],\n"
				<< "  \"schema\": \"angelscript-standalone-result/1.0\",\n"
				<< "  \"status\": "
				<< EscapeJsonString(
					Result.bSuccess
						? ToString(Result.CapabilitySummary.Completeness)
						: "failed")
				<< ",\n"
				<< "  \"symbolScope\": "
				<< ScopeJson(Result.SymbolScope) << ",\n"
				<< "  \"ueValidationOnly\": true,\n"
				<< "  \"upstreamLineage\": "
				<< EscapeJsonString(
					UNREAL_ANGELSCRIPT_UPSTREAM_LINEAGE_STRING)
				<< "\n"
				<< "}\n";
			return Json.str();
		}
	}

	FValidationArtifactWriteResult WriteValidationArtifacts(
		const FValidationArtifactWriteRequest& Request)
	{
		FValidationArtifactWriteResult Result;
		if (Request.CompileResult == nullptr
			|| Request.OutputDirectory.empty()
			|| Request.CompileResult->ModuleId.empty())
		{
			Result.Error =
				"UE-validation artifact request is incomplete";
			return Result;
		}

		const FUECompileResult& Compile = *Request.CompileResult;
		const std::filesystem::path Staging =
			Request.OutputDirectory.parent_path()
			/ (Request.OutputDirectory.filename().string() + ".staging");
		const std::filesystem::path Backup =
			Request.OutputDirectory.parent_path()
			/ (Request.OutputDirectory.filename().string() + ".previous");
		std::error_code ErrorCode;
		std::filesystem::remove_all(Staging, ErrorCode);
		ErrorCode.clear();
		std::filesystem::create_directories(
			Staging / "modules",
			ErrorCode);
		if (ErrorCode)
		{
			Result.Error =
				"failed to create UE-validation artifact staging directory";
			return Result;
		}

		const std::filesystem::path ModuleRoot =
			Staging / "modules" / Compile.ModuleId;
		if (!WriteText(Staging / "result.json", ResultJson(Compile))
			|| !WriteText(
				Staging / "diagnostics.jsonl",
				DiagnosticsJsonLines(Compile.Diagnostics))
			|| !WriteText(
				ModuleRoot.string() + ".classes.jsonl",
				ClassesJsonLines(Compile))
			|| (!Compile.ByteCode.empty()
				&& !WriteBinary(
					ModuleRoot.string() + ".asbc",
					Compile.ByteCode)))
		{
			std::filesystem::remove_all(Staging, ErrorCode);
			Result.Error = "failed to write UE-validation artifacts";
			return Result;
		}

		std::filesystem::remove_all(Backup, ErrorCode);
		ErrorCode.clear();
		if (std::filesystem::exists(Request.OutputDirectory))
		{
			std::filesystem::rename(
				Request.OutputDirectory,
				Backup,
				ErrorCode);
			if (ErrorCode)
			{
				std::filesystem::remove_all(Staging, ErrorCode);
				Result.Error =
					"failed to prepare atomic UE-validation artifact replacement";
				return Result;
			}
		}
		std::filesystem::rename(
			Staging,
			Request.OutputDirectory,
			ErrorCode);
		if (ErrorCode)
		{
			std::error_code RollbackError;
			if (std::filesystem::exists(Backup))
			{
				std::filesystem::rename(
					Backup,
					Request.OutputDirectory,
					RollbackError);
			}
			std::filesystem::remove_all(Staging, RollbackError);
			Result.Error =
				"failed to publish UE-validation artifacts";
			return Result;
		}
		std::filesystem::remove_all(Backup, ErrorCode);
		Result.bSuccess = true;
		return Result;
	}
}
