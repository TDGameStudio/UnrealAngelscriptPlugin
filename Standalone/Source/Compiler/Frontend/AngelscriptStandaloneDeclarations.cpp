#include "Compiler/Frontend/AngelscriptStandaloneDeclarations.h"

#include "Compiler/Frontend/AngelscriptStandaloneLexing.h"
#include "Support/AngelscriptStandaloneHash.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <set>

namespace AngelscriptStandalone::Frontend
{
	namespace
	{
		struct FOwnerScope
		{
			FByteOffset Begin = 0;
			FByteOffset End = 0;
			std::string Name;
		};

		std::string_view Trim(std::string_view Text)
		{
			while (!Text.empty()
				&& std::isspace(
					static_cast<unsigned char>(Text.front())) != 0)
			{
				Text.remove_prefix(1);
			}
			while (!Text.empty()
				&& std::isspace(
					static_cast<unsigned char>(Text.back())) != 0)
			{
				Text.remove_suffix(1);
			}
			return Text;
		}

		bool IsIdentifierCharacter(const char Character)
		{
			return std::isalnum(
					static_cast<unsigned char>(Character)) != 0
				|| Character == '_';
		}

		bool IsWordAt(
			const std::string_view Text,
			const std::vector<bool>& Code,
			const FByteOffset Offset,
			const std::string_view Word)
		{
			if (Offset + Word.size() > Text.size()
				|| Text.substr(Offset, Word.size()) != Word)
			{
				return false;
			}
			for (FByteOffset Index = Offset;
				Index < Offset + Word.size();
				++Index)
			{
				if (!Code[Index])
				{
					return false;
				}
			}
			return (Offset == 0
					|| !IsIdentifierCharacter(Text[Offset - 1]))
				&& (Offset + Word.size() == Text.size()
					|| !IsIdentifierCharacter(
						Text[Offset + Word.size()]));
		}

		FByteOffset SkipWhitespace(
			const std::string_view Text,
			FByteOffset Offset)
		{
			while (Offset < Text.size()
				&& std::isspace(
					static_cast<unsigned char>(Text[Offset])) != 0)
			{
				++Offset;
			}
			return Offset;
		}

		std::vector<std::string_view> SplitTopLevel(
			const std::string_view Text,
			const char Delimiter)
		{
			std::vector<std::string_view> Result;
			int Parentheses = 0;
			int Angles = 0;
			int Brackets = 0;
			bool bString = false;
			bool bEscaped = false;
			std::size_t Start = 0;
			for (std::size_t Index = 0; Index < Text.size(); ++Index)
			{
				const char Character = Text[Index];
				if (bString)
				{
					if (bEscaped)
					{
						bEscaped = false;
					}
					else if (Character == '\\')
					{
						bEscaped = true;
					}
					else if (Character == '"')
					{
						bString = false;
					}
					continue;
				}
				if (Character == '"')
				{
					bString = true;
				}
				else if (Character == '(')
				{
					++Parentheses;
				}
				else if (Character == ')')
				{
					--Parentheses;
				}
				else if (Character == '<')
				{
					++Angles;
				}
				else if (Character == '>')
				{
					Angles = std::max(0, Angles - 1);
				}
				else if (Character == '[')
				{
					++Brackets;
				}
				else if (Character == ']')
				{
					--Brackets;
				}
				else if (Character == Delimiter
					&& Parentheses == 0
					&& Angles == 0
					&& Brackets == 0)
				{
					Result.push_back(Trim(Text.substr(Start, Index - Start)));
					Start = Index + 1;
				}
			}
			Result.push_back(Trim(Text.substr(Start)));
			return Result;
		}

		std::vector<FMetadataEntry> ParseMetadata(
			const std::string_view Text)
		{
			std::vector<FMetadataEntry> Result;
			for (const std::string_view Item : SplitTopLevel(Text, ','))
			{
				if (Item.empty())
				{
					continue;
				}
				const std::size_t Equals = Item.find('=');
				FMetadataEntry& Entry = Result.emplace_back();
				Entry.Name = std::string(Trim(Item.substr(0, Equals)));
				if (Equals != std::string_view::npos)
				{
					Entry.Value =
						std::string(Trim(Item.substr(Equals + 1)));
				}
			}
			return Result;
		}

		std::string LastIdentifier(
			const std::string_view Text,
			std::size_t& OutBegin)
		{
			std::size_t End = Text.size();
			while (End > 0
				&& !IsIdentifierCharacter(Text[End - 1]))
			{
				--End;
			}
			std::size_t Begin = End;
			while (Begin > 0
				&& IsIdentifierCharacter(Text[Begin - 1]))
			{
				--Begin;
			}
			OutBegin = Begin;
			return std::string(Text.substr(Begin, End - Begin));
		}

		FTypeReference ParseType(std::string_view Text)
		{
			FTypeReference Result;
			Text = Trim(Text);
			if (Text.starts_with("const "))
			{
				Result.bConst = true;
				Text.remove_prefix(6);
				Text = Trim(Text);
			}
			if (Text.ends_with("&inout"))
			{
				Result.bReference = true;
				Text.remove_suffix(6);
			}
			else if (Text.ends_with("&out"))
			{
				Result.bReference = true;
				Text.remove_suffix(4);
			}
			else if (Text.ends_with("&in"))
			{
				Result.bReference = true;
				Text.remove_suffix(3);
			}
			else if (Text.ends_with('&'))
			{
				Result.bReference = true;
				Text.remove_suffix(1);
			}
			if (Text.ends_with('@'))
			{
				Result.bHandle = true;
				Text.remove_suffix(1);
			}
			Result.Spelling = std::string(Trim(Text));
			return Result;
		}

		bool ConsumeWord(
			std::string_view& Text,
			const std::string_view Word)
		{
			Text = Trim(Text);
			if (!Text.starts_with(Word)
				|| (Text.size() > Word.size()
					&& IsIdentifierCharacter(Text[Word.size()])))
			{
				return false;
			}
			Text.remove_prefix(Word.size());
			Text = Trim(Text);
			return true;
		}

		EAccess ConsumeAccess(std::string_view& Text)
		{
			if (ConsumeWord(Text, "public"))
			{
				return EAccess::Public;
			}
			if (ConsumeWord(Text, "protected"))
			{
				return EAccess::Protected;
			}
			if (ConsumeWord(Text, "private"))
			{
				return EAccess::Private;
			}
			return EAccess::Unspecified;
		}

		std::vector<FParameterDeclaration> ParseParameters(
			const std::string_view Text)
		{
			std::vector<FParameterDeclaration> Result;
			for (std::string_view Item : SplitTopLevel(Text, ','))
			{
				if (Item.empty() || Item == "void")
				{
					continue;
				}
				const std::vector<std::string_view> DefaultParts =
					SplitTopLevel(Item, '=');
				const std::string_view Left = DefaultParts.front();
				std::size_t NameBegin = 0;
				std::string Name = LastIdentifier(Left, NameBegin);
				FParameterDeclaration& Parameter = Result.emplace_back();
				if (NameBegin == 0)
				{
					Parameter.Type = ParseType(Left);
				}
				else
				{
					Parameter.Name = std::move(Name);
					Parameter.Type = ParseType(Left.substr(0, NameBegin));
				}
				if (DefaultParts.size() > 1)
				{
					const std::size_t Equals = Item.find('=');
					Parameter.DefaultValue =
						std::string(Trim(Item.substr(Equals + 1)));
				}
			}
			return Result;
		}

		FByteOffset FindDeclarationEnd(
			const std::string_view Text,
			const std::vector<bool>& Code,
			const FByteOffset Begin)
		{
			int Parentheses = 0;
			int Angles = 0;
			for (FByteOffset Offset = Begin;
				Offset < Text.size();
				++Offset)
			{
				if (!Code[Offset])
					continue;
				const char Character = Text[Offset];
				Parentheses += Character == '(' ? 1 : 0;
				Parentheses -= Character == ')' ? 1 : 0;
				Angles += Character == '<' ? 1 : 0;
				Angles -= Character == '>' && Angles > 0 ? 1 : 0;
				if (Parentheses == 0 && Angles == 0
					&& (Character == ';' || Character == '{'))
				{
					return Offset;
				}
			}
			return Text.size();
		}

		std::string FindOwner(
			const std::vector<FOwnerScope>& Scopes,
			const FByteOffset Offset)
		{
			std::string Result;
			FByteOffset BestSize = static_cast<FByteOffset>(-1);
			for (const FOwnerScope& Scope : Scopes)
			{
				if (Scope.Begin < Offset && Offset < Scope.End
					&& Scope.End - Scope.Begin < BestSize)
				{
					Result = Scope.Name;
					BestSize = Scope.End - Scope.Begin;
				}
			}
			return Result;
		}

		void AddDiagnostic(
			FDeclarationScanResult& Result,
			std::string Code,
			std::string Message,
			const FSourceSpan Span)
		{
			Result.Diagnostics.push_back({
				std::move(Code),
				std::move(Message),
				Span,
			});
		}

		bool IsBuiltinType(const std::string_view Type)
		{
			static const std::set<std::string_view> Builtins = {
				"bool", "double", "float", "int", "int8", "int16",
				"int32", "int64", "uint", "uint8", "uint16", "uint32",
				"uint64", "void",
			};
			return Builtins.contains(Type);
		}

		const char* DeclarationKindName(const EDeclarationKind Kind)
		{
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

		bool ResolveTypeReference(
			FTypeReference& Type,
			const std::map<std::string, std::string>& LocalTypes,
			const ITypeOracle& Oracle)
		{
			if (Type.Spelling.empty())
			{
				return false;
			}
			if (IsBuiltinType(Type.Spelling))
			{
				Type.StableTypeId = "builtin:" + Type.Spelling;
				return true;
			}
			const auto Local = LocalTypes.find(Type.Spelling);
			if (Local != LocalTypes.end())
			{
				Type.StableTypeId = Local->second;
				return true;
			}
			if (Oracle.ResolveType(Type.Spelling, Type.StableTypeId))
			{
				return true;
			}
			const std::size_t Open = Type.Spelling.find('<');
			if (Open == std::string::npos
				|| !Type.Spelling.ends_with('>'))
			{
				return false;
			}
			const std::string Base = Type.Spelling.substr(0, Open);
			std::string BaseId;
			if (!Oracle.ResolveType(Base, BaseId))
			{
				return false;
			}
			for (const std::string_view Argument : SplitTopLevel(
					std::string_view(Type.Spelling).substr(
						Open + 1,
						Type.Spelling.size() - Open - 2),
					','))
			{
				FTypeReference Subtype = ParseType(Argument);
				if (!ResolveTypeReference(Subtype, LocalTypes, Oracle))
				{
					return false;
				}
			}
			Type.StableTypeId = BaseId + ":specialization:"
				+ Type.Spelling;
			return true;
		}
	}

	FDeclarationScanResult ScanDeclarations(
		const FSourceInput& Source,
		const std::string_view PreprocessedSource)
	{
		FDeclarationScanResult Result;
		const std::vector<FLexicalRange> Ranges =
			ScanLexicalRanges(PreprocessedSource);
		std::vector<bool> Code(PreprocessedSource.size(), false);
		for (const FLexicalRange& Range : Ranges)
		{
			if (Range.Kind == ELexicalRangeKind::Code)
			{
				for (FByteOffset Offset = Range.Span.Begin;
					Offset < Range.Span.End && Offset < Code.size();
					++Offset)
				{
					Code[Offset] = true;
				}
			}
		}

		struct FMacro
		{
			std::string Name;
			FSourceSpan Span;
			FByteOffset After = 0;
			std::vector<FMetadataEntry> Metadata;
		};
		std::vector<FMacro> Macros;
		const std::array<std::string_view, 5> MacroNames = {
			"UCLASS", "USTRUCT", "UENUM", "UFUNCTION", "UPROPERTY",
		};
		for (FByteOffset Offset = 0;
			Offset < PreprocessedSource.size();
			++Offset)
		{
			for (const std::string_view Name : MacroNames)
			{
				if (!IsWordAt(PreprocessedSource, Code, Offset, Name))
					continue;
				const FByteOffset Open =
					SkipWhitespace(PreprocessedSource, Offset + Name.size());
				if (Open >= PreprocessedSource.size()
					|| PreprocessedSource[Open] != '(')
				{
					AddDiagnostic(
						Result,
						"ASL-DECL-ANNOTATION-SYNTAX",
						std::string(Name) + " must be followed by '(...)'",
						{Offset, Offset + Name.size()});
					continue;
				}
				const FDelimiterMatch Close = FindMatchingDelimiter(
					PreprocessedSource,
					Open,
					'(',
					')');
				if (!Close.bSuccess)
				{
					AddDiagnostic(
						Result,
						"ASL-DECL-ANNOTATION-SYNTAX",
						Close.Error,
						{Offset, Open + 1});
					continue;
				}
				FMacro& Macro = Macros.emplace_back();
				Macro.Name = std::string(Name);
				Macro.Span = {Offset, Close.CloseOffset + 1};
				Macro.After = Close.CloseOffset + 1;
				Macro.Metadata = ParseMetadata(
					PreprocessedSource.substr(
						Open + 1,
						Close.CloseOffset - Open - 1));
				Result.AnnotationSpans.push_back(Macro.Span);
				Offset = Close.CloseOffset;
				break;
			}
		}

		std::vector<FOwnerScope> OwnerScopes;
		for (const FMacro& Macro : Macros)
		{
			if (Macro.Name != "UCLASS"
				&& Macro.Name != "USTRUCT"
				&& Macro.Name != "UENUM")
			{
				continue;
			}
			const FByteOffset Begin =
				SkipWhitespace(PreprocessedSource, Macro.After);
			const std::string_view Keyword =
				Macro.Name == "UCLASS"
					? "class"
					: Macro.Name == "USTRUCT" ? "struct" : "enum";
			if (!IsWordAt(
					PreprocessedSource,
					Code,
					Begin,
					Keyword))
			{
				AddDiagnostic(
					Result,
					"ASL-DECL-ANNOTATION-TARGET",
					Macro.Name + " does not annotate a "
						+ std::string(Keyword),
					Macro.Span);
				continue;
			}
			const FByteOffset HeaderEnd = FindDeclarationEnd(
				PreprocessedSource,
				Code,
				Begin);
			if (HeaderEnd >= PreprocessedSource.size()
				|| PreprocessedSource[HeaderEnd] != '{')
			{
				AddDiagnostic(
					Result,
					"ASL-DECL-TYPE-SYNTAX",
					"annotated type has no body",
					{Begin, HeaderEnd});
				continue;
			}
			const FDelimiterMatch Body = FindMatchingDelimiter(
				PreprocessedSource,
				HeaderEnd,
				'{',
				'}');
			if (!Body.bSuccess)
			{
				AddDiagnostic(
					Result,
					"ASL-DECL-TYPE-SYNTAX",
					Body.Error,
					{HeaderEnd, HeaderEnd + 1});
				continue;
			}

			std::string_view Header = Trim(
				PreprocessedSource.substr(
					Begin + Keyword.size(),
					HeaderEnd - Begin - Keyword.size()));
			if (Keyword == "enum" && Header.starts_with("class "))
			{
				Header.remove_prefix(6);
				Header = Trim(Header);
			}
			std::size_t NameEnd = 0;
			while (NameEnd < Header.size()
				&& IsIdentifierCharacter(Header[NameEnd]))
			{
				++NameEnd;
			}
			if (NameEnd == 0)
			{
				AddDiagnostic(
					Result,
					"ASL-DECL-TYPE-SYNTAX",
					"annotated type has no name",
					{Begin, HeaderEnd});
				continue;
			}

			FDeclaration& Declaration = Result.Declarations.emplace_back();
			Declaration.Kind =
				Macro.Name == "UCLASS"
					? EDeclarationKind::Class
					: Macro.Name == "USTRUCT"
						? EDeclarationKind::Struct
						: EDeclarationKind::Enum;
			Declaration.Name = std::string(Header.substr(0, NameEnd));
			Declaration.QualifiedName = Declaration.Name;
			Declaration.Declaration =
				std::string(Trim(PreprocessedSource.substr(
					Begin,
					HeaderEnd - Begin)));
			Declaration.Metadata = Macro.Metadata;
			Declaration.Type.Spelling = Declaration.Name;
			Declaration.Type.StableTypeId =
				"source-type:"
				+ MakeStableModuleId(Source.LogicalPath)
				+ ":" + Declaration.Name;
			Declaration.Source = {
				Source.LogicalPath,
				Begin,
				Body.CloseOffset + 1,
			};
			if (Declaration.Kind == EDeclarationKind::Enum)
			{
				const std::string_view EnumBody =
					PreprocessedSource.substr(
						HeaderEnd + 1,
						Body.CloseOffset - HeaderEnd - 1);
				for (const std::string_view Item
					: SplitTopLevel(EnumBody, ','))
				{
					const std::string_view Entry = Trim(Item);
					if (Entry.empty())
					{
						continue;
					}
					const std::size_t Equals = Entry.find('=');
					FEnumValueDeclaration& Value =
						Declaration.EnumValues.emplace_back();
					Value.Name = std::string(
						Trim(Entry.substr(0, Equals)));
					if (Equals != std::string_view::npos)
					{
						Value.ValueExpression = std::string(
							Trim(Entry.substr(Equals + 1)));
					}
				}
			}
			const std::size_t Colon = Header.find(':', NameEnd);
			if (Colon != std::string_view::npos
				&& Declaration.Kind != EDeclarationKind::Enum)
			{
				for (const std::string_view Base :
					SplitTopLevel(Header.substr(Colon + 1), ','))
				{
					if (!Base.empty())
					{
						Declaration.BaseTypes.push_back(ParseType(Base));
					}
				}
			}
			OwnerScopes.push_back({
				HeaderEnd,
				Body.CloseOffset,
				Declaration.Name,
			});
		}

		// Plain AngelScript type declarations participate in the same
		// declaration/type IR as UE-annotated declarations. This is required
		// for a later annotated member to reference a local script enum,
		// class, or struct without consulting a second parser.
		std::set<FByteOffset> TypeDeclarationBegins;
		for (const FDeclaration& Declaration : Result.Declarations)
		{
			if (Declaration.Kind == EDeclarationKind::Class
				|| Declaration.Kind == EDeclarationKind::Struct
				|| Declaration.Kind == EDeclarationKind::Enum)
			{
				TypeDeclarationBegins.insert(Declaration.Source.Begin);
			}
		}
		const std::array<std::string_view, 3> TypeKeywords = {
			"class", "struct", "enum",
		};
		for (FByteOffset Offset = 0;
			Offset < PreprocessedSource.size();
			++Offset)
		{
			for (const std::string_view Keyword : TypeKeywords)
			{
				if (!IsWordAt(
						PreprocessedSource,
						Code,
						Offset,
						Keyword)
					|| TypeDeclarationBegins.contains(Offset))
				{
					continue;
				}
				// In "enum class EName", class is part of the enum header,
				// not a second declaration.
				if (Keyword == "class")
				{
					FByteOffset Previous = Offset;
					while (Previous > 0
						&& std::isspace(static_cast<unsigned char>(
							PreprocessedSource[Previous - 1])))
					{
						--Previous;
					}
					FByteOffset PreviousBegin = Previous;
					while (PreviousBegin > 0
						&& IsIdentifierCharacter(
							PreprocessedSource[PreviousBegin - 1]))
					{
						--PreviousBegin;
					}
					if (PreprocessedSource.substr(
							PreviousBegin,
							Previous - PreviousBegin) == "enum")
					{
						continue;
					}
				}

				const FByteOffset HeaderEnd = FindDeclarationEnd(
					PreprocessedSource,
					Code,
					Offset);
				if (HeaderEnd >= PreprocessedSource.size()
					|| PreprocessedSource[HeaderEnd] != '{')
				{
					continue;
				}
				const FDelimiterMatch Body = FindMatchingDelimiter(
					PreprocessedSource,
					HeaderEnd,
					'{',
					'}');
				if (!Body.bSuccess)
				{
					AddDiagnostic(
						Result,
						"ASL-DECL-TYPE-SYNTAX",
						Body.Error,
						{HeaderEnd, HeaderEnd + 1});
					continue;
				}
				std::string_view Header = Trim(
					PreprocessedSource.substr(
						Offset + Keyword.size(),
						HeaderEnd - Offset - Keyword.size()));
				if (Keyword == "enum" && Header.starts_with("class "))
				{
					Header.remove_prefix(6);
					Header = Trim(Header);
				}
				std::size_t NameEnd = 0;
				while (NameEnd < Header.size()
					&& IsIdentifierCharacter(Header[NameEnd]))
				{
					++NameEnd;
				}
				if (NameEnd == 0)
				{
					continue;
				}

				FDeclaration& Declaration =
					Result.Declarations.emplace_back();
				Declaration.Kind = Keyword == "class"
					? EDeclarationKind::Class
					: Keyword == "struct"
						? EDeclarationKind::Struct
						: EDeclarationKind::Enum;
				Declaration.Name =
					std::string(Header.substr(0, NameEnd));
				Declaration.QualifiedName = Declaration.Name;
				Declaration.Declaration = std::string(
					Trim(PreprocessedSource.substr(
						Offset,
						HeaderEnd - Offset)));
				Declaration.Type.Spelling = Declaration.Name;
				Declaration.Type.StableTypeId =
					"source-type:"
					+ MakeStableModuleId(Source.LogicalPath)
					+ ":" + Declaration.Name;
				Declaration.Source = {
					Source.LogicalPath,
					Offset,
					Body.CloseOffset + 1,
				};
				if (Declaration.Kind == EDeclarationKind::Enum)
				{
					const std::string_view EnumBody =
						PreprocessedSource.substr(
							HeaderEnd + 1,
							Body.CloseOffset - HeaderEnd - 1);
					for (const std::string_view Item
						: SplitTopLevel(EnumBody, ','))
					{
						const std::string_view Entry = Trim(Item);
						if (Entry.empty())
						{
							continue;
						}
						const std::size_t Equals = Entry.find('=');
						FEnumValueDeclaration& Value =
							Declaration.EnumValues.emplace_back();
						Value.Name = std::string(
							Trim(Entry.substr(0, Equals)));
						if (Equals != std::string_view::npos)
						{
							Value.ValueExpression = std::string(
								Trim(Entry.substr(Equals + 1)));
						}
					}
				}
				const std::size_t Colon =
					Header.find(':', NameEnd);
				if (Colon != std::string_view::npos
					&& Declaration.Kind != EDeclarationKind::Enum)
				{
					for (const std::string_view Base
						: SplitTopLevel(Header.substr(Colon + 1), ','))
					{
						if (!Base.empty())
						{
							Declaration.BaseTypes.push_back(
								ParseType(Base));
						}
					}
				}
				TypeDeclarationBegins.insert(Offset);
				OwnerScopes.push_back({
					HeaderEnd,
					Body.CloseOffset,
					Declaration.Name,
				});
				Offset = Body.CloseOffset;
				break;
			}
		}

		for (const FMacro& Macro : Macros)
		{
			if (Macro.Name != "UFUNCTION"
				&& Macro.Name != "UPROPERTY")
			{
				continue;
			}
			const FByteOffset Begin =
				SkipWhitespace(PreprocessedSource, Macro.After);
			const FByteOffset End = FindDeclarationEnd(
				PreprocessedSource,
				Code,
				Begin);
			if (End >= PreprocessedSource.size())
			{
				AddDiagnostic(
					Result,
					"ASL-DECL-MEMBER-SYNTAX",
					Macro.Name + " has no following declaration",
					Macro.Span);
				continue;
			}
			const std::string_view Statement = Trim(
				PreprocessedSource.substr(Begin, End - Begin));
			FDeclaration& Declaration = Result.Declarations.emplace_back();
			Declaration.Metadata = Macro.Metadata;
			Declaration.Owner = FindOwner(OwnerScopes, Begin);
			Declaration.Source = {Source.LogicalPath, Begin, End + 1};
			Declaration.Declaration = std::string(Statement);

			if (Macro.Name == "UPROPERTY")
			{
				Declaration.Kind = Declaration.Owner.empty()
					? EDeclarationKind::Global
					: EDeclarationKind::Property;
				const std::vector<std::string_view> DefaultParts =
					SplitTopLevel(Statement, '=');
				std::string_view Left = DefaultParts.front();
				Declaration.Access = ConsumeAccess(Left);
				Declaration.bStatic = ConsumeWord(Left, "static");
				if (Declaration.Access == EAccess::Unspecified)
				{
					Declaration.Access = ConsumeAccess(Left);
				}
				std::size_t NameBegin = 0;
				Declaration.Name = LastIdentifier(Left, NameBegin);
				Declaration.Type = ParseType(Left.substr(0, NameBegin));
				if (DefaultParts.size() > 1)
				{
					const std::size_t Equals = Statement.find('=');
					Declaration.DefaultValue =
						std::string(Trim(Statement.substr(Equals + 1)));
				}
			}
			else
			{
				Declaration.Kind = EDeclarationKind::Function;
				const std::size_t Open = Statement.find('(');
				if (Open == std::string_view::npos)
				{
					AddDiagnostic(
						Result,
						"ASL-DECL-FUNCTION-SYNTAX",
						"UFUNCTION target has no parameter list",
						{Begin, End});
					Result.Declarations.pop_back();
					continue;
				}
				const FDelimiterMatch Close = FindMatchingDelimiter(
					Statement,
					Open,
					'(',
					')');
				if (!Close.bSuccess)
				{
					AddDiagnostic(
						Result,
						"ASL-DECL-FUNCTION-SYNTAX",
						Close.Error,
						{Begin + Open, Begin + Open + 1});
					Result.Declarations.pop_back();
					continue;
				}
				std::string_view Before = Trim(
					Statement.substr(0, Open));
				Declaration.Access = ConsumeAccess(Before);
				Declaration.bStatic = ConsumeWord(Before, "static");
				if (Declaration.Access == EAccess::Unspecified)
				{
					Declaration.Access = ConsumeAccess(Before);
				}
				std::size_t NameBegin = 0;
				Declaration.Name =
					LastIdentifier(Before, NameBegin);
				Declaration.Type =
					ParseType(Before.substr(0, NameBegin));
				Declaration.Parameters = ParseParameters(
					Statement.substr(
						Open + 1,
						Close.CloseOffset - Open - 1));
				const std::string_view Suffix = Trim(
					Statement.substr(Close.CloseOffset + 1));
				Declaration.bConst =
					Suffix.find("const") != std::string_view::npos;
				Declaration.bOverride =
					Suffix.find("override") != std::string_view::npos;
				Declaration.bFinal =
					Suffix.find("final") != std::string_view::npos;
				for (const FMetadataEntry& Metadata : Declaration.Metadata)
				{
					if (Metadata.Name == "BlueprintEvent"
						|| Metadata.Name == "BlueprintOverride"
						|| Metadata.Name == "Server"
						|| Metadata.Name == "Client"
						|| Metadata.Name == "NetMulticast")
					{
						Declaration.Kind = EDeclarationKind::Event;
					}
				}
			}
			Declaration.QualifiedName = Declaration.Owner.empty()
				? Declaration.Name
				: Declaration.Owner + "::" + Declaration.Name;
		}

		for (FByteOffset Offset = 0;
			Offset < PreprocessedSource.size();
			++Offset)
		{
			bool bEvent = false;
			if (!IsWordAt(
					PreprocessedSource,
					Code,
					Offset,
					"delegate")
				&& !(bEvent = IsWordAt(
					PreprocessedSource,
					Code,
					Offset,
					"event")))
			{
				continue;
			}
			const std::string_view Keyword = bEvent ? "event" : "delegate";
			const FByteOffset Begin = Offset;
			const FByteOffset End = FindDeclarationEnd(
				PreprocessedSource,
				Code,
				Begin);
			if (End >= PreprocessedSource.size()
				|| PreprocessedSource[End] != ';')
			{
				continue;
			}
			const std::string_view Statement = Trim(
				PreprocessedSource.substr(
					Begin + Keyword.size(),
					End - Begin - Keyword.size()));
			const std::size_t Open = Statement.find('(');
			if (Open == std::string_view::npos)
				continue;
			const FDelimiterMatch Close =
				FindMatchingDelimiter(Statement, Open, '(', ')');
			if (!Close.bSuccess)
				continue;
			const std::string_view Before = Trim(
				Statement.substr(0, Open));
			std::size_t NameBegin = 0;
			FDeclaration& Declaration = Result.Declarations.emplace_back();
			Declaration.Kind = bEvent
				? EDeclarationKind::Event
				: EDeclarationKind::Delegate;
			Declaration.Name = LastIdentifier(Before, NameBegin);
			Declaration.QualifiedName = Declaration.Name;
			Declaration.Type =
				ParseType(Before.substr(0, NameBegin));
			Declaration.Parameters = ParseParameters(
				Statement.substr(
					Open + 1,
					Close.CloseOffset - Open - 1));
			Declaration.Declaration = std::string(
				Trim(PreprocessedSource.substr(Begin, End - Begin)));
			Declaration.Source = {Source.LogicalPath, Begin, End + 1};
			Offset = End;
		}

		std::sort(
			Result.Declarations.begin(),
			Result.Declarations.end(),
			[](const FDeclaration& Left, const FDeclaration& Right)
			{
				if (Left.Source.Begin != Right.Source.Begin)
				{
					return Left.Source.Begin < Right.Source.Begin;
				}
				return Left.QualifiedName < Right.QualifiedName;
			});
		const std::string StableModuleId =
			MakeStableModuleId(Source.LogicalPath);
		for (FDeclaration& Declaration : Result.Declarations)
		{
			Declaration.ModuleId = StableModuleId;
			Declaration.StableId = Sha256(
				std::string("source-declaration-v1\n")
				+ StableModuleId + "\n"
				+ DeclarationKindName(Declaration.Kind) + "\n"
				+ Declaration.QualifiedName + "\n"
				+ Declaration.Declaration);
		}
		Result.bSuccess = Result.Diagnostics.empty();
		return Result;
	}

	FTypeResolutionResult ResolveDeclarationTypes(
		std::vector<FDeclaration>& Declarations,
		const ITypeOracle& Oracle)
	{
		FTypeResolutionResult Result;
		std::map<std::string, std::string> LocalTypes;
		for (const FDeclaration& Declaration : Declarations)
		{
			const bool bNominalDeclaration =
				Declaration.Kind == EDeclarationKind::Class
				|| Declaration.Kind == EDeclarationKind::Struct
				|| Declaration.Kind == EDeclarationKind::Enum
				|| Declaration.Kind == EDeclarationKind::Delegate
				|| (Declaration.Kind == EDeclarationKind::Event
					&& Declaration.Owner.empty()
					&& Declaration.Declaration.starts_with("event "));
			if (bNominalDeclaration)
			{
				const bool bCallableNominalType =
					Declaration.Kind == EDeclarationKind::Delegate
					|| Declaration.Kind == EDeclarationKind::Event;
				const std::string StableTypeId =
					bCallableNominalType
						? "source-type:" + Declaration.ModuleId
							+ ":" + Declaration.Name
						: Declaration.Type.StableTypeId;
				LocalTypes.emplace(
					Declaration.Name,
					StableTypeId);
				LocalTypes.emplace(
					Declaration.QualifiedName,
					StableTypeId);
			}
		}

		for (FDeclaration& Declaration : Declarations)
		{
			const auto Resolve = [&](FTypeReference& Type)
			{
				if (!ResolveTypeReference(Type, LocalTypes, Oracle))
				{
					Result.Diagnostics.push_back({
						"ASL-TYPE-UNKNOWN",
						"unknown type: " + Type.Spelling,
						{
							Declaration.Source.Begin,
							Declaration.Source.Begin
								+ Type.Spelling.size(),
						},
					});
				}
			};
			if (Declaration.Kind == EDeclarationKind::Property
				|| Declaration.Kind == EDeclarationKind::Global
				|| Declaration.Kind == EDeclarationKind::Function
				|| Declaration.Kind == EDeclarationKind::Event
				|| Declaration.Kind == EDeclarationKind::Delegate)
			{
				Resolve(Declaration.Type);
			}
			for (FTypeReference& Base : Declaration.BaseTypes)
			{
				Resolve(Base);
			}
			for (FParameterDeclaration& Parameter : Declaration.Parameters)
			{
				Resolve(Parameter.Type);
			}
		}
		Result.bSuccess = Result.Diagnostics.empty();
		return Result;
	}
}
