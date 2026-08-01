#include "Resources/AngelscriptResourceContext.h"

#include "Compiler/Frontend/AngelscriptStandaloneLexing.h"
#include "Support/AngelscriptStandaloneHash.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <set>
#include <string_view>

namespace AngelscriptStandalone
{
	namespace
	{
		std::string_view Trim(std::string_view Text)
		{
			while (!Text.empty()
				&& std::isspace(
					static_cast<unsigned char>(Text.front())) != 0)
				Text.remove_prefix(1);
			while (!Text.empty()
				&& std::isspace(
					static_cast<unsigned char>(Text.back())) != 0)
				Text.remove_suffix(1);
			return Text;
		}

		bool IsIdentifierCharacter(const char Character)
		{
			return std::isalnum(
				static_cast<unsigned char>(Character)) != 0
				|| Character == '_';
		}

		bool EvaluateConstantString(
			std::string_view Expression,
			const std::map<std::string, std::string>& Constants,
			std::string& OutValue)
		{
			Expression = Trim(Expression);
			OutValue.clear();
			std::size_t Offset = 0;
			std::size_t TermCount = 0;
			while (Offset < Expression.size())
			{
				while (Offset < Expression.size()
					&& std::isspace(
						static_cast<unsigned char>(
							Expression[Offset])) != 0)
					++Offset;
				if (Offset >= Expression.size())
					return false;
				if (++TermCount > 64)
					return false;
				if (Expression[Offset] == '"')
				{
					++Offset;
					bool bClosed = false;
					while (Offset < Expression.size())
					{
						const char Character = Expression[Offset++];
						if (Character == '"')
						{
							bClosed = true;
							break;
						}
						if (Character == '\\')
						{
							if (Offset >= Expression.size())
								return false;
							const char Escaped = Expression[Offset++];
							if (Escaped == '\\' || Escaped == '"'
								|| Escaped == '\'')
								OutValue.push_back(Escaped);
							else
								return false;
						}
						else
						{
							OutValue.push_back(Character);
						}
					}
					if (!bClosed)
						return false;
				}
				else if (IsIdentifierCharacter(Expression[Offset])
					&& !std::isdigit(
						static_cast<unsigned char>(
							Expression[Offset])))
				{
					const std::size_t NameBegin = Offset++;
					while (Offset < Expression.size()
						&& IsIdentifierCharacter(Expression[Offset]))
					{
						++Offset;
					}
					const auto Found = Constants.find(std::string(
						Expression.substr(
							NameBegin,
							Offset - NameBegin)));
					if (Found == Constants.end())
						return false;
					OutValue += Found->second;
				}
				else
				{
					return false;
				}
				if (OutValue.size() > 4096)
					return false;
				while (Offset < Expression.size()
					&& std::isspace(
						static_cast<unsigned char>(
							Expression[Offset])) != 0)
					++Offset;
				if (Offset == Expression.size())
					break;
				if (Expression[Offset++] != '+')
					return false;
			}
			return TermCount != 0;
		}

		bool EvaluateConstantString(
			const std::string_view Expression,
			std::string& OutValue)
		{
			static const std::map<std::string, std::string> NoConstants;
			return EvaluateConstantString(
				Expression,
				NoConstants,
				OutValue);
		}

		const FOfflineSymbolRecord* ResolveTypeRecord(
			std::string_view StableTypeId,
			const FOfflineBundleIndices& Indices)
		{
			const std::size_t Specialization =
				StableTypeId.find(":specialization:");
			if (Specialization != std::string_view::npos)
				StableTypeId = StableTypeId.substr(0, Specialization);
			return Indices.FindType(StableTypeId);
		}

		const FOfflineSymbolRecord* FindUniqueTypeByName(
			const std::string_view Name,
			const FOfflineBundleIndices& Indices)
		{
			const FOfflineSymbolRecord* Result = nullptr;
			for (const FOfflineSymbolRecord& Symbol : Indices.Symbols())
			{
				if (!Symbol.Type.StableId.empty()
					&& Symbol.Type.Name == Name)
				{
					if (Result != nullptr)
						return nullptr;
					Result = &Symbol;
				}
			}
			return Result;
		}

		std::string TemplateArgument(std::string_view Type)
		{
			const std::size_t Open = Type.find('<');
			if (Open == std::string_view::npos
				|| !Type.ends_with('>'))
				return {};
			return std::string(Trim(Type.substr(
				Open + 1,
				Type.size() - Open - 2)));
		}

		bool ClassifyTypeContext(
			const AngelscriptStandalone::Frontend::FTypeReference& Type,
			const FOfflineBundleIndices& Indices,
			EResourceContextKind& OutKind,
			std::string& OutRequestedType)
		{
			const FOfflineSymbolRecord* TypeRecord =
				ResolveTypeRecord(Type.StableTypeId, Indices);
			const std::string Name = TypeRecord != nullptr
				? TypeRecord->Type.Name
				: Type.Spelling.substr(0, Type.Spelling.find('<'));
			if (Name == "FSoftObjectPath"
				|| Name == "TSoftObjectPtr")
				OutKind = EResourceContextKind::SoftObject;
			else if (Name == "FSoftClassPath"
				|| Name == "TSoftClassPtr")
				OutKind = EResourceContextKind::SoftClass;
			else
				return false;
			const std::string Argument =
				TemplateArgument(Type.Spelling);
			if (!Argument.empty())
			{
				if (const FOfflineSymbolRecord* Requested =
					FindUniqueTypeByName(Argument, Indices))
				{
					OutRequestedType = Requested->StableId;
				}
			}
			return true;
		}

		bool IsCodeOffset(
			const std::vector<AngelscriptStandalone::Frontend::FLexicalRange>&
				Ranges,
			const std::size_t Offset)
		{
			const auto Iterator = std::lower_bound(
				Ranges.begin(),
				Ranges.end(),
				Offset,
				[](const auto& Range, const std::size_t Value)
				{
					return Range.Span.End <= Value;
				});
			return Iterator != Ranges.end()
				&& Iterator->Span.Begin <= Offset
				&& Offset < Iterator->Span.End
				&& Iterator->Kind
					== AngelscriptStandalone::Frontend::ELexicalRangeKind::Code;
		}

		std::size_t SkipWhitespace(
			const std::string_view Source,
			std::size_t Offset)
		{
			while (Offset < Source.size()
				&& std::isspace(
					static_cast<unsigned char>(Source[Offset])) != 0)
			{
				++Offset;
			}
			return Offset;
		}

		std::map<std::string, std::string> CollectConstantStrings(
			const std::string_view Source,
			const std::vector<AngelscriptStandalone::Frontend::FLexicalRange>&
				Ranges)
		{
			std::map<std::string, std::string> Constants;
			std::size_t Search = 0;
			while (Search < Source.size())
			{
				const std::size_t Candidate =
					Source.find("const", Search);
				if (Candidate == std::string_view::npos)
					break;
				Search = Candidate + 5;
				if (!IsCodeOffset(Ranges, Candidate)
					|| (Candidate > 0
						&& IsIdentifierCharacter(Source[Candidate - 1]))
					|| (Candidate + 5 < Source.size()
						&& IsIdentifierCharacter(Source[Candidate + 5])))
					continue;

				std::size_t Equals = std::string_view::npos;
				std::size_t End = std::string_view::npos;
				for (std::size_t Cursor = Candidate + 5;
					Cursor < Source.size();
					++Cursor)
				{
					if (!IsCodeOffset(Ranges, Cursor))
						continue;
					const char Character = Source[Cursor];
					if (Character == '(' || Character == ')'
						|| Character == '{' || Character == '}'
						|| Character == ',')
					{
						End = Cursor;
						break;
					}
					if (Character == '='
						&& Equals == std::string_view::npos)
						Equals = Cursor;
					if (Character == ';')
					{
						End = Cursor;
						break;
					}
				}
				if (Equals == std::string_view::npos
					|| End == std::string_view::npos
					|| Source[End] != ';')
					continue;
				std::string_view Left = Trim(Source.substr(
					Candidate + 5,
					Equals - Candidate - 5));
				std::size_t NameBegin = Left.size();
				while (NameBegin > 0
					&& IsIdentifierCharacter(Left[NameBegin - 1]))
					--NameBegin;
				const std::string Name(
					Left.substr(NameBegin));
				if (Name.empty())
					continue;
				std::string Value;
				if (EvaluateConstantString(
						Source.substr(
							Equals + 1,
							End - Equals - 1),
						Constants,
						Value))
				{
					Constants.insert_or_assign(Name, std::move(Value));
				}
			}
			return Constants;
		}

		std::vector<AngelscriptStandalone::Frontend::FSourceSpan> SplitArguments(
			const std::string_view Source,
			const std::size_t Open,
			const std::size_t Close,
			const std::vector<AngelscriptStandalone::Frontend::FLexicalRange>&
				Ranges)
		{
			using AngelscriptStandalone::Frontend::FSourceSpan;
			std::vector<FSourceSpan> Arguments;
			std::size_t Begin = Open + 1;
			int Parentheses = 0;
			int Brackets = 0;
			int Braces = 0;
			int Templates = 0;
			for (std::size_t Cursor = Begin; Cursor < Close; ++Cursor)
			{
				if (!IsCodeOffset(Ranges, Cursor))
					continue;
				switch (Source[Cursor])
				{
				case '(':
					++Parentheses;
					break;
				case ')':
					Parentheses = std::max(0, Parentheses - 1);
					break;
				case '[':
					++Brackets;
					break;
				case ']':
					Brackets = std::max(0, Brackets - 1);
					break;
				case '{':
					++Braces;
					break;
				case '}':
					Braces = std::max(0, Braces - 1);
					break;
				case '<':
					++Templates;
					break;
				case '>':
					Templates = std::max(0, Templates - 1);
					break;
				case ',':
					if (Parentheses == 0 && Brackets == 0
						&& Braces == 0 && Templates == 0)
					{
						Arguments.push_back({Begin, Cursor});
						Begin = Cursor + 1;
					}
					break;
				default:
					break;
				}
			}
			if (Begin < Close || !Arguments.empty())
				Arguments.push_back({Begin, Close});
			for (FSourceSpan& Span : Arguments)
			{
				while (Span.Begin < Span.End
					&& std::isspace(static_cast<unsigned char>(
						Source[Span.Begin])) != 0)
					++Span.Begin;
				while (Span.End > Span.Begin
					&& std::isspace(static_cast<unsigned char>(
						Source[Span.End - 1])) != 0)
					--Span.End;
			}
			return Arguments;
		}

		bool ClassifyCall(
			const std::string_view Name,
			EResourceContextKind& OutKind,
			bool& bOutSoft,
			bool& bOutClass,
			bool& bOutType)
		{
			bOutSoft = true;
			bOutClass = false;
			bOutType = true;
			if (Name == "FSoftObjectPath"
				|| Name == "TSoftObjectPtr")
				OutKind = EResourceContextKind::SoftObject;
			else if (Name == "FSoftClassPath"
				|| Name == "TSoftClassPtr")
			{
				OutKind = EResourceContextKind::SoftClass;
				bOutClass = true;
			}
			else if (Name == "LoadObject")
			{
				OutKind = EResourceContextKind::LoadObject;
				bOutSoft = false;
				bOutType = false;
			}
			else if (Name == "LoadClass")
			{
				OutKind = EResourceContextKind::LoadClass;
				bOutSoft = false;
				bOutClass = true;
				bOutType = false;
			}
			else
				return false;
			return true;
		}

		bool ClassifyMarkedResourceKind(
			const std::string_view Mark,
			EResourceContextKind& OutKind,
			bool& bOutSoft,
			bool& bOutClass)
		{
			if (Mark == "soft-object")
			{
				OutKind = EResourceContextKind::SoftObject;
				bOutSoft = true;
				bOutClass = false;
			}
			else if (Mark == "soft-class")
			{
				OutKind = EResourceContextKind::SoftClass;
				bOutSoft = true;
				bOutClass = true;
			}
			else if (Mark == "load-object")
			{
				OutKind = EResourceContextKind::LoadObject;
				bOutSoft = false;
				bOutClass = false;
			}
			else if (Mark == "load-class")
			{
				OutKind = EResourceContextKind::LoadClass;
				bOutSoft = false;
				bOutClass = true;
			}
			else
			{
				return false;
			}
			return true;
		}

		const FOfflineSymbolRecord* FindContextSymbol(
			const std::string_view Name,
			const bool bType,
			const std::size_t ArgumentCount,
			const FOfflineBundleIndices& Indices)
		{
			const FOfflineSymbolRecord* Match = nullptr;
			for (const FOfflineSymbolRecord& Symbol : Indices.Symbols())
			{
				bool bMatches = bType
					? (!Symbol.Type.StableId.empty()
						&& Symbol.Type.Name == Name)
					: (!Symbol.Callable.StableId.empty()
						&& Symbol.Callable.Name == Name);
				if (!bMatches)
					continue;
				if (!bType)
				{
					std::size_t Required = 0;
					for (const auto& Parameter
						: Symbol.Callable.Parameters)
						Required += Parameter.bHasDefault ? 0 : 1;
					bMatches =
						Required <= ArgumentCount
						&& ArgumentCount
							<= Symbol.Callable.Parameters.size();
				}
				if (!bMatches)
					continue;
				if (Match != nullptr)
					return nullptr;
				Match = &Symbol;
			}
			return Match;
		}

		std::size_t FindPathArgument(
			const FOfflineSymbolRecord& ContextSymbol,
			const bool bType,
			const std::size_t ArgumentCount)
		{
			if (bType)
				return ArgumentCount == 0
					? std::string_view::npos
					: 0;
			for (std::size_t Index =
					ContextSymbol.Callable.Parameters.size();
				Index > 0;
				--Index)
			{
				const auto& Parameter =
					ContextSymbol.Callable.Parameters[Index - 1];
				std::string LowerName = Parameter.Name;
				std::transform(
					LowerName.begin(),
					LowerName.end(),
					LowerName.begin(),
					[](const unsigned char Character)
					{
						return static_cast<char>(
							std::tolower(Character));
					});
				if (LowerName.find("path") != std::string::npos
					|| LowerName == "name"
					|| LowerName == "asset")
					return Index - 1;
			}
			return ArgumentCount == 1
				? 0
				: std::string_view::npos;
		}

		bool IsTypeStableIdMatch(
			const std::string_view Observed,
			const std::string_view Expected)
		{
			return Observed == Expected
				|| (Observed.size() > Expected.size()
					&& Observed.starts_with(Expected)
					&& Observed.substr(Expected.size())
						.starts_with(":specialization:"));
		}

		bool HasResolvedSemanticContext(
			const std::vector<FSemanticObservation>& Observations,
			const std::string_view LogicalPath,
			const std::size_t Begin,
			const std::size_t End,
			const FOfflineSymbolRecord& ContextSymbol,
			const bool bTypeContext)
		{
			return std::any_of(
				Observations.begin(),
				Observations.end(),
				[&](const FSemanticObservation& Observation)
				{
					if (Observation.LogicalPath != LogicalPath)
					{
						return false;
					}
					const bool bCallSpanMatches =
						Observation.Source.Begin >= Begin
						&& Observation.Source.Begin < End;
					const bool bArgumentSpanMatches =
						std::any_of(
							Observation.Arguments.begin(),
							Observation.Arguments.end(),
							[&](const FSemanticArgumentObservation&
									Argument)
							{
								return Argument.Source.Begin >= Begin
									&& Argument.Source.Begin < End
									&& Argument.Source.End <= End;
							});
					if (!bCallSpanMatches
						&& !bArgumentSpanMatches)
					{
						return false;
					}
					if (bTypeContext)
					{
						return Observation.Kind
								== ESemanticObservationKind::Constructor
							&& IsTypeStableIdMatch(
								Observation.TargetStableTypeId,
								ContextSymbol.StableId);
					}
					return Observation.Kind
							== ESemanticObservationKind::ResolvedCall
						&& Observation.StableFunctionId
							== ContextSymbol.StableId;
				});
		}

		void FinalizeContext(FResourceContext& Context)
		{
			Context.ContextId = Sha256(
				"resource-context-v1\n"
				+ Context.LogicalPath + "\n"
				+ std::to_string(Context.Span.Begin) + "\n"
				+ Context.ContextStableSymbolId + "\n"
				+ Context.ConstantPath);
		}
	}

	FResourceContextResult DiscoverResourceContexts(
		const std::vector<AngelscriptStandalone::Frontend::FLanguageModule>&
			Modules,
		const std::vector<AngelscriptStandalone::Frontend::FDeclaration>&
			Declarations,
		const FOfflineBundleIndices& Indices,
		const std::vector<FSemanticObservation>*
			SemanticObservations)
	{
		using namespace Frontend;
		FResourceContextResult Result;
		std::set<std::string> Seen;
		for (const FDeclaration& Declaration : Declarations)
		{
			if (Declaration.DefaultValue.empty())
				continue;
			EResourceContextKind Kind =
				EResourceContextKind::SoftObject;
			std::string RequestedType;
			if (!ClassifyTypeContext(
					Declaration.Type,
					Indices,
					Kind,
					RequestedType))
				continue;
			std::string Path;
			if (!EvaluateConstantString(
					Declaration.DefaultValue,
					Path))
				continue;
			FResourceContext Context;
			Context.ContextStableSymbolId =
				Declaration.StableId;
			Context.LogicalPath =
				Declaration.Source.LogicalPath;
			Context.Span = {
				Declaration.Source.Begin,
				Declaration.Source.End,
			};
			Context.Kind = Kind;
			Context.OriginalExpression =
				Declaration.DefaultValue;
			Context.ConstantPath = std::move(Path);
			Context.RequestedStableTypeId =
				std::move(RequestedType);
			Context.bClass =
				Kind == EResourceContextKind::SoftClass;
			FinalizeContext(Context);
			if (Seen.emplace(Context.ContextId).second)
				Result.Contexts.push_back(std::move(Context));
		}

		for (const FLanguageModule& Module : Modules)
		{
			const std::vector<FLexicalRange> Ranges =
				ScanLexicalRanges(Module.ProcessedSource);
			const std::map<std::string, std::string> Constants =
				CollectConstantStrings(
					Module.ProcessedSource,
					Ranges);

			// Parameter markers are exported from the final reflected or
			// manually registered callable surface. Consume them only through
			// a compiler-resolved stable callable ID and compiler-provided
			// argument spans; spelling alone never authorizes a resource
			// lookup.
			if (SemanticObservations != nullptr)
			{
				for (const FSemanticObservation& Observation
					: *SemanticObservations)
				{
					if (Observation.Kind
							!= ESemanticObservationKind::ResolvedCall
						|| Observation.LogicalPath != Module.LogicalPath
						|| Observation.StableFunctionId.empty())
					{
						continue;
					}
					const FOfflineSymbolRecord* Callable =
						Indices.FindCallable(
							Observation.StableFunctionId);
					if (Callable == nullptr)
					{
						continue;
					}
					const std::size_t ParameterCount = std::min(
						Callable->Callable.Parameters.size(),
						Observation.Arguments.size());
					for (std::size_t ParameterIndex = 0;
						ParameterIndex < ParameterCount;
						++ParameterIndex)
					{
						const auto& Parameter =
							Callable->Callable.Parameters[
								ParameterIndex];
						if (Parameter.ResourceKind.empty())
						{
							continue;
						}
						EResourceContextKind Kind =
							EResourceContextKind::SoftObject;
						bool bSoft = true;
						bool bClass = false;
						if (!ClassifyMarkedResourceKind(
								Parameter.ResourceKind,
								Kind,
								bSoft,
								bClass))
						{
							continue;
						}

						const FSourceSpan Span =
							Observation.Arguments[
								ParameterIndex].Source;
						if (Span.Begin > Span.End
							|| Span.End
								> Module.ProcessedSource.size())
						{
							continue;
						}
						const std::string_view Expression =
							std::string_view(
								Module.ProcessedSource)
								.substr(
									Span.Begin,
									Span.End - Span.Begin);
						std::string Path;
						if (!EvaluateConstantString(
								Expression,
								Constants,
								Path))
						{
							continue;
						}

						FResourceContext Context;
						Context.ContextStableSymbolId =
							Callable->StableId;
						Context.LogicalPath =
							Module.LogicalPath;
						Context.Span = Span;
						Context.Kind = Kind;
						Context.OriginalExpression =
							std::string(Expression);
						Context.ConstantPath = std::move(Path);
						Context.RequestedStableTypeId =
							Parameter.ResourceTypeStableId;
						Context.bSoft = bSoft;
						Context.bClass = bClass;
						FinalizeContext(Context);
						if (Seen.emplace(Context.ContextId).second)
						{
							Result.Contexts.push_back(
								std::move(Context));
						}
					}
				}
			}

			std::set<std::string> ShadowedHostCallables;
			for (const FDeclaration& Declaration : Declarations)
			{
				if (Declaration.Kind != EDeclarationKind::Function
					&& Declaration.Kind != EDeclarationKind::Event)
					continue;
				if ((!Declaration.ModuleId.empty()
						&& Declaration.ModuleId == Module.ModuleId)
					|| (!Declaration.Source.LogicalPath.empty()
						&& Declaration.Source.LogicalPath
							== Module.LogicalPath))
				{
					ShadowedHostCallables.emplace(Declaration.Name);
				}
			}

			static constexpr std::array<std::string_view, 6>
				KnownContexts = {
					"FSoftObjectPath",
					"FSoftClassPath",
					"TSoftObjectPtr",
					"TSoftClassPtr",
					"LoadObject",
					"LoadClass",
				};
			for (const std::string_view Call : KnownContexts)
			{
				std::size_t Search = 0;
				while (Search < Module.ProcessedSource.size())
				{
					const std::size_t Candidate =
						Module.ProcessedSource.find(Call, Search);
					if (Candidate == std::string::npos)
						break;
					Search = Candidate + Call.size();
					if (!IsCodeOffset(Ranges, Candidate)
						|| (Candidate > 0
							&& IsIdentifierCharacter(
								Module.ProcessedSource[
									Candidate - 1]))
						|| (Candidate + Call.size()
								< Module.ProcessedSource.size()
							&& IsIdentifierCharacter(
								Module.ProcessedSource[
									Candidate + Call.size()])))
						continue;

					EResourceContextKind Kind =
						EResourceContextKind::SoftObject;
					bool bSoft = true;
					bool bClass = false;
					bool bTypeContext = false;
					if (!ClassifyCall(
							Call,
							Kind,
							bSoft,
							bClass,
							bTypeContext))
						continue;
					if (!bTypeContext
						&& ShadowedHostCallables.contains(
							std::string(Call)))
						continue;

					std::size_t Cursor =
						SkipWhitespace(
							Module.ProcessedSource,
							Candidate + Call.size());
					std::string TypeSpelling(Call);
					if (Cursor < Module.ProcessedSource.size()
						&& Module.ProcessedSource[Cursor] == '<')
					{
						const FDelimiterMatch Template =
							FindMatchingDelimiter(
								Module.ProcessedSource,
								Cursor,
								'<',
								'>');
						if (!Template.bSuccess)
							continue;
						TypeSpelling = std::string(
							std::string_view(Module.ProcessedSource)
								.substr(
									Candidate,
									Template.CloseOffset
										- Candidate + 1));
						Cursor = SkipWhitespace(
							Module.ProcessedSource,
							Template.CloseOffset + 1);
					}
					if (bTypeContext
						&& Cursor < Module.ProcessedSource.size()
						&& IsIdentifierCharacter(
							Module.ProcessedSource[Cursor])
						&& !std::isdigit(
							static_cast<unsigned char>(
								Module.ProcessedSource[Cursor])))
					{
						while (Cursor < Module.ProcessedSource.size()
							&& IsIdentifierCharacter(
								Module.ProcessedSource[Cursor]))
							++Cursor;
						Cursor = SkipWhitespace(
							Module.ProcessedSource,
							Cursor);
					}
					if (Cursor >= Module.ProcessedSource.size()
						|| Module.ProcessedSource[Cursor] != '('
						|| !IsCodeOffset(Ranges, Cursor))
						continue;
					const FDelimiterMatch CallDelimiter =
						FindMatchingDelimiter(
							Module.ProcessedSource,
							Cursor,
							'(',
							')');
					if (!CallDelimiter.bSuccess)
						continue;
					const std::vector<FSourceSpan> Arguments =
						SplitArguments(
							Module.ProcessedSource,
							Cursor,
							CallDelimiter.CloseOffset,
							Ranges);
					const FOfflineSymbolRecord* ContextSymbol =
						FindContextSymbol(
							Call,
							bTypeContext,
							Arguments.size(),
							Indices);
					if (ContextSymbol == nullptr)
						continue;
					if (SemanticObservations != nullptr
						&& !HasResolvedSemanticContext(
							*SemanticObservations,
							Module.LogicalPath,
							Candidate,
							CallDelimiter.CloseOffset + 1,
							*ContextSymbol,
							bTypeContext))
					{
						continue;
					}
					const std::size_t PathArgument =
						FindPathArgument(
							*ContextSymbol,
							bTypeContext,
							Arguments.size());
					if (PathArgument == std::string_view::npos
						|| PathArgument >= Arguments.size())
						continue;
					const FSourceSpan ExpressionSpan =
						Arguments[PathArgument];
					const std::string_view Expression =
						std::string_view(Module.ProcessedSource)
							.substr(
								ExpressionSpan.Begin,
								ExpressionSpan.End
									- ExpressionSpan.Begin);
					std::string Path;
					if (!EvaluateConstantString(
							Expression,
							Constants,
							Path))
						continue;

					FResourceContext Context;
					Context.ContextStableSymbolId =
						ContextSymbol->StableId;
					Context.LogicalPath = Module.LogicalPath;
					Context.Span = ExpressionSpan;
					Context.Kind = Kind;
					Context.OriginalExpression =
						std::string(Expression);
					Context.ConstantPath = std::move(Path);
					Context.bSoft = bSoft;
					Context.bClass = bClass;
					const std::string Requested =
						TemplateArgument(TypeSpelling);
					if (!Requested.empty())
					{
						if (const FOfflineSymbolRecord* RequestedType =
							FindUniqueTypeByName(
								Requested,
								Indices))
						{
							Context.RequestedStableTypeId =
								RequestedType->StableId;
						}
					}
					FinalizeContext(Context);
					if (Seen.emplace(Context.ContextId).second)
					{
						Result.Contexts.push_back(
							std::move(Context));
					}
					Search = CallDelimiter.CloseOffset + 1;
				}
			}
		}
		std::sort(
			Result.Contexts.begin(),
			Result.Contexts.end(),
			[](const auto& Left, const auto& Right)
			{
				return Left.ContextId < Right.ContextId;
			});
		Result.bSuccess = true;
		return Result;
	}
}
