#include "Compiler/AngelscriptStandaloneDeclarationAnalysis.h"

#include "Compiler/Frontend/AngelscriptStandaloneRewrite.h"

#include <algorithm>
#include <map>
#include <set>
#include <string_view>

namespace AngelscriptStandalone
{
	namespace
	{
		const char* DeclarationRule(
			const AngelscriptStandalone::Frontend::EDeclarationKind Kind)
		{
			using AngelscriptStandalone::Frontend::EDeclarationKind;
			switch (Kind)
			{
			case EDeclarationKind::Class:
				return "source-class-semantics";
			case EDeclarationKind::Struct:
				return "source-struct-semantics";
			case EDeclarationKind::Enum:
				return "source-enum-semantics";
			case EDeclarationKind::Delegate:
				return "source-delegate-semantics";
			case EDeclarationKind::Property:
				return "source-property-semantics";
			case EDeclarationKind::Function:
				return "source-function-semantics";
			case EDeclarationKind::Event:
				return "source-event-semantics";
			case EDeclarationKind::Global:
			default:
				return "source-global-semantics";
			}
		}

		void ClassifyMetadata(
			const AngelscriptStandalone::Frontend::FDeclaration& Declaration,
			std::vector<FCapabilityObservation>& Capabilities)
		{
			static const std::set<std::string_view> UERuntimeMetadata = {
				"Attach",
				"Client",
				"DefaultComponent",
				"NetMulticast",
				"OverrideComponent",
				"Replicated",
				"ReplicatedUsing",
				"RootComponent",
				"Server",
				"WithValidation",
			};
			for (const auto& Metadata : Declaration.Metadata)
			{
				if (UERuntimeMetadata.contains(Metadata.Name))
				{
					Capabilities.push_back({
						Declaration.StableId,
						"ue-runtime-metadata:" + Metadata.Name,
						ECapabilityClassification::UERequired,
						"the annotation is syntax-visible offline, but its "
						"runtime/reflection effect requires Unreal Engine",
					});
				}
				else if (Metadata.Name == "BlueprintEvent"
					|| Metadata.Name == "BlueprintOverride")
				{
					Capabilities.push_back({
						Declaration.StableId,
						"blueprint-dispatch:" + Metadata.Name,
						ECapabilityClassification::CompileShim,
						"the signature is validated; Blueprint dispatch "
						"and ProcessEvent execution are unavailable",
					});
				}
			}
		}

		std::string DefaultReturnExpression(
			const AngelscriptStandalone::Frontend::FTypeReference& Type)
		{
			if (Type.Spelling == "bool")
			{
				return "false";
			}
			if (Type.bHandle)
			{
				return "null";
			}
			static const std::set<std::string_view> NumericTypes = {
				"double", "float", "int", "int8", "int16", "int32",
				"int64", "uint", "uint8", "uint16", "uint32", "uint64",
			};
			if (NumericTypes.contains(Type.Spelling))
			{
				return "0";
			}
			return Type.Spelling + "()";
		}

		std::string BlankPreservingLines(const std::string_view Text)
		{
			std::string Result(Text);
			for (char& Character : Result)
			{
				if (Character != '\r' && Character != '\n')
				{
					Character = ' ';
				}
			}
			return Result;
		}
	}

	FDeclarationAnalysisResult AnalyzeStandaloneDeclarations(
		std::vector<AngelscriptStandalone::Frontend::FLanguageModule>& Modules,
		const AngelscriptStandalone::Frontend::ITypeOracle& TypeOracle)
	{
		using namespace Frontend;
		FDeclarationAnalysisResult Result;
		for (const FLanguageModule& Module : Modules)
		{
			Result.Declarations.insert(
				Result.Declarations.end(),
				Module.Declarations.begin(),
				Module.Declarations.end());
		}

		const FTypeResolutionResult Resolution =
			ResolveDeclarationTypes(Result.Declarations, TypeOracle);
		if (!Resolution.bSuccess)
		{
			for (const FDeclarationDiagnostic& Source
				: Resolution.Diagnostics)
			{
				std::string LogicalPath;
				for (const FDeclaration& Declaration
					: Result.Declarations)
				{
					if (Declaration.Source.Begin <= Source.Span.Begin
						&& Source.Span.Begin <= Declaration.Source.End)
					{
						LogicalPath = Declaration.Source.LogicalPath;
						break;
					}
				}
				Result.Diagnostics.push_back({
					Source.Code,
					Source.Message,
					std::move(LogicalPath),
					Source.Span,
					EDiagnosticSeverity::Error,
				});
			}
			Result.Error =
				"UE declaration semantic analysis failed";
			return Result;
		}

		std::map<std::string, FDeclaration> ResolvedByStableId;
		for (const FDeclaration& Declaration : Result.Declarations)
		{
			if (!ResolvedByStableId.emplace(
					Declaration.StableId,
					Declaration).second)
			{
				Result.Error =
					"duplicate source declaration stable ID: "
					+ Declaration.StableId;
				return Result;
			}
			Result.Capabilities.push_back({
				Declaration.StableId,
				DeclarationRule(Declaration.Kind),
				ECapabilityClassification::Exact,
				"declaration syntax and bundle-backed type identities "
					"were resolved by the portable language core",
			});
			ClassifyMetadata(Declaration, Result.Capabilities);
		}
		for (FLanguageModule& Module : Modules)
		{
			for (FDeclaration& Declaration : Module.Declarations)
			{
				const auto Found =
					ResolvedByStableId.find(Declaration.StableId);
				if (Found != ResolvedByStableId.end())
				{
					Declaration = Found->second;
				}
			}
		}

		for (FLanguageModule& Module : Modules)
		{
			FRewritePlan Lowering;
			for (const FDeclaration& Declaration
				: Module.Declarations)
			{
				const bool bSourceDelegate =
					Declaration.Declaration.starts_with("delegate ");
				const bool bSourceEvent =
					Declaration.Declaration.starts_with("event ");
				if (bSourceDelegate || bSourceEvent)
				{
					const FRewriteOperationResult Edit =
						Lowering.Add({
							Declaration.Source.Begin,
							Declaration.Source.End,
							BlankPreservingLines(
								std::string_view(Module.ProcessedSource)
									.substr(
										Declaration.Source.Begin,
										Declaration.Source.End
											- Declaration.Source.Begin)),
						});
					if (!Edit.bSuccess)
					{
						Result.Error = Edit.Error;
						return Result;
					}
					Result.Capabilities.push_back({
						Declaration.StableId,
						"delegate-signature-lowering",
						ECapabilityClassification::CompileShim,
						"the UE delegate/event source syntax is removed "
							"and its signature is registered as a compile-only "
							"funcdef; binding and broadcast execution are "
							"unavailable",
					});
				}

				const bool bBlueprintOverride =
					std::any_of(
						Declaration.Metadata.begin(),
						Declaration.Metadata.end(),
						[](const FMetadataEntry& Metadata)
						{
							return Metadata.Name
								== "BlueprintOverride";
						});
				if (bBlueprintOverride && !bSourceEvent)
				{
					const std::size_t NameOffset =
						Module.ProcessedSource.find(
							Declaration.Name,
							Declaration.Source.Begin);
					if (NameOffset == std::string::npos
						|| NameOffset + Declaration.Name.size()
							> Declaration.Source.End)
					{
						Result.Error =
							"could not locate BlueprintOverride method name: "
							+ Declaration.QualifiedName;
						return Result;
					}
					const FRewriteOperationResult Edit =
						Lowering.Add({
							NameOffset,
							NameOffset + Declaration.Name.size(),
							Declaration.Name
								+ "__standalone_override_"
								+ Declaration.StableId.substr(0, 12),
						});
					if (!Edit.bSuccess)
					{
						Result.Error = Edit.Error;
						return Result;
					}
					Result.Capabilities.push_back({
						Declaration.StableId,
						"blueprint-override-shadow",
						ECapabilityClassification::CompileShim,
						"the BlueprintOverride body is compiled under a "
							"validation-only name to avoid colliding with "
							"the imported host shadow method",
					});
				}

				if (Declaration.Kind == EDeclarationKind::Class)
				{
					std::vector<const FTypeReference*> LocalBases;
					std::vector<const FTypeReference*> HostBases;
					for (const FTypeReference& Base
						: Declaration.BaseTypes)
					{
						if (Base.StableTypeId.starts_with(
								"source-type:"))
						{
							LocalBases.push_back(&Base);
						}
						else
						{
							HostBases.push_back(&Base);
						}
					}
					if (!HostBases.empty())
					{
						Result.PreClassBindings.push_back({
							Declaration.StableId,
							Declaration.Name,
							HostBases.front()->StableTypeId,
						});
						const std::size_t Body =
							Module.ProcessedSource.find(
								'{',
								Declaration.Source.Begin);
						const std::size_t Colon =
							Module.ProcessedSource.find(
								':',
								Declaration.Source.Begin);
						if (Body == std::string::npos
							|| Colon == std::string::npos
							|| Colon >= Body)
						{
							Result.Error =
								"could not lower external base list for "
								+ Declaration.QualifiedName;
							return Result;
						}
						std::string Replacement =
							BlankPreservingLines(
								std::string_view(Module.ProcessedSource)
									.substr(Colon, Body - Colon));
						if (!LocalBases.empty())
						{
							std::string Clause = ": ";
							for (std::size_t Index = 0;
								Index < LocalBases.size();
								++Index)
							{
								if (Index != 0)
								{
									Clause += ", ";
								}
								Clause += LocalBases[Index]->Spelling;
							}
							if (Clause.size() <= Replacement.size())
							{
								Replacement.replace(
									0,
									Clause.size(),
									Clause);
							}
							else
							{
								Replacement = std::move(Clause);
							}
						}
						const FRewriteOperationResult Edit =
							Lowering.Add({
								Colon,
								Body,
								std::move(Replacement),
							});
						if (!Edit.bSuccess)
						{
							Result.Error = Edit.Error;
							return Result;
						}
						Result.Capabilities.push_back({
							Declaration.StableId,
							"host-base-shadow",
							ECapabilityClassification::CompileShim,
							"the source class uses compile-only pre-class "
								"shadow metadata for its exported host base",
						});
						for (std::size_t Index = 1;
							Index < HostBases.size();
							++Index)
						{
							Result.Capabilities.push_back({
								Declaration.StableId,
								"additional-host-interface",
								ECapabilityClassification::UERequired,
								"additional exported host interfaces are "
									"validated in IR but require Unreal "
									"reflection for full member projection",
							});
						}
					}
				}

				if (Declaration.Kind == EDeclarationKind::Event
					&& !bSourceEvent
					&& Declaration.Source.End != 0)
				{
					const std::size_t Semicolon =
						Declaration.Source.End - 1;
					if (Semicolon < Module.ProcessedSource.size()
						&& Module.ProcessedSource[Semicolon] == ';')
					{
						std::string Body = "{}";
						if (Declaration.Type.Spelling != "void")
						{
							Body = "{ return "
								+ DefaultReturnExpression(Declaration.Type)
								+ "; }";
						}
						const FRewriteOperationResult Edit =
							Lowering.Add({
								Semicolon,
								Semicolon + 1,
								std::move(Body),
							});
						if (!Edit.bSuccess)
						{
							Result.Error = Edit.Error;
							return Result;
						}
						Result.Capabilities.push_back({
							Declaration.StableId,
							"event-body-lowering",
							ECapabilityClassification::CompileShim,
							"declaration-only UE event body is lowered for "
								"compile-time validation; UE execution is "
								"forbidden",
						});
					}
				}
			}
			if (!Lowering.GetEdits().empty())
			{
				const FRewriteResult Rewritten =
					Lowering.Apply(Module.ProcessedSource);
				if (!Rewritten.bSuccess)
				{
					Result.Error = Rewritten.Error;
					return Result;
				}
				Module.ProcessedSource = Rewritten.Text;
			}
			const FRewriteResult NameLiterals =
				RewriteNameLiterals(Module.ProcessedSource);
			if (!NameLiterals.bSuccess)
			{
				Result.Error = NameLiterals.Error;
				return Result;
			}
			const FRewriteResult RangeFor =
				RewriteRangeBasedFor(NameLiterals.Text);
			if (!RangeFor.bSuccess)
			{
				Result.Error = RangeFor.Error;
				return Result;
			}
			Module.ProcessedSource = RangeFor.Text;
		}

		std::sort(
			Result.Declarations.begin(),
			Result.Declarations.end(),
			[](const FDeclaration& Left, const FDeclaration& Right)
			{
				return Left.StableId < Right.StableId;
			});
		std::sort(
			Result.Capabilities.begin(),
			Result.Capabilities.end(),
			[](const FCapabilityObservation& Left,
				const FCapabilityObservation& Right)
			{
				if (Left.StableId != Right.StableId)
				{
					return Left.StableId < Right.StableId;
				}
				return Left.Rule < Right.Rule;
			});
		Result.bSuccess = true;
		return Result;
	}
}
