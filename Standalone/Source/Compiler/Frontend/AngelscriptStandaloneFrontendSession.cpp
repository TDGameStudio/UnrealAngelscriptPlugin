#include "Compiler/Frontend/AngelscriptStandaloneFrontendSession.h"

#include "Compiler/Frontend/AngelscriptStandaloneLexing.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <set>
#include <sstream>

namespace AngelscriptStandalone::Frontend
{
	namespace
	{
		struct FConditionalFrame
		{
			bool bParentActive = true;
			bool bBranchTaken = false;
			bool bCurrentActive = true;
			bool bSawElse = false;
			FByteOffset DirectiveOffset = 0;
		};

		std::string_view TrimLanguageSession(std::string_view Text)
		{
			while (!Text.empty() && (Text.front() == ' ' || Text.front() == '\t'))
			{
				Text.remove_prefix(1);
			}
			while (!Text.empty()
				&& (Text.back() == ' ' || Text.back() == '\t' || Text.back() == '\r'))
			{
				Text.remove_suffix(1);
			}
			return Text;
		}

		void BlankRange(std::string& Text, FByteOffset Begin, FByteOffset End)
		{
			for (FByteOffset Offset = Begin; Offset < End && Offset < Text.size(); ++Offset)
			{
				if (Text[Offset] != '\n' && Text[Offset] != '\r')
				{
					Text[Offset] = ' ';
				}
			}
		}

		bool IsCodeOffset(const std::vector<FLexicalRange>& Ranges, FByteOffset Offset)
		{
			const auto Iterator = std::lower_bound(
				Ranges.begin(),
				Ranges.end(),
				Offset,
				[](const FLexicalRange& Range, FByteOffset Value)
				{
					return Range.Span.End <= Value;
				});
			return Iterator != Ranges.end()
				&& Iterator->Span.Begin <= Offset
				&& Offset < Iterator->Span.End
				&& Iterator->Kind == ELexicalRangeKind::Code;
		}

		void AddDiagnostic(
			FPreprocessResult& Result,
			std::string Code,
			std::string Message,
			FSourceSpan Span)
		{
			Result.Diagnostics.push_back({
				std::move(Code),
				std::move(Message),
				Result.LogicalPath,
				Span,
				EDiagnosticSeverity::Error,
			});
		}

		bool EvaluateCondition(
			std::string_view Expression,
			const FPreprocessConfig& Config,
			bool& OutValue,
			std::string& OutUnknownFlag)
		{
			Expression = TrimLanguageSession(Expression);
			bool bNegated = false;
			if (!Expression.empty() && Expression.front() == '!')
			{
				bNegated = true;
				Expression.remove_prefix(1);
				Expression = TrimLanguageSession(Expression);
			}
			if (Expression.empty()
				|| Expression.find_first_not_of(
					"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")
					!= std::string_view::npos)
			{
				OutUnknownFlag = std::string(Expression);
				return false;
			}
			const auto Flag = Config.Flags.find(std::string(Expression));
			if (Flag == Config.Flags.end())
			{
				OutUnknownFlag = std::string(Expression);
				return false;
			}
			OutValue = Flag->second != bNegated;
			return true;
		}

		bool ParseImportModule(std::string_view Line, std::string& OutModule)
		{
			Line = TrimLanguageSession(Line);
			if (!Line.starts_with("import"))
			{
				return false;
			}
			Line.remove_prefix(6);
			if (Line.empty() || (Line.front() != ' ' && Line.front() != '\t'))
			{
				return false;
			}
			Line = TrimLanguageSession(Line);
			const std::size_t Semicolon = Line.find(';');
			if (Semicolon == std::string_view::npos
				|| !TrimLanguageSession(
					Line.substr(Semicolon + 1)).empty())
			{
				return false;
			}
			const std::string_view Module =
				TrimLanguageSession(Line.substr(0, Semicolon));
			if (Module.empty()
				|| Module.find_first_not_of(
					"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.")
					!= std::string_view::npos)
			{
				return false;
			}
			OutModule.assign(Module);
			return true;
		}
	}

	FPreprocessResult PreprocessSource(
		const FSourceInput& Source,
		const FPreprocessConfig& Config)
	{
		FPreprocessResult Result;
		const FPathResult NormalizedPath = NormalizeLogicalPath(Source.LogicalPath);
		if (!NormalizedPath.bSuccess)
		{
			Result.LogicalPath = Source.LogicalPath;
			AddDiagnostic(
				Result,
				"ASL-SOURCE-PATH",
				NormalizedPath.Error,
				{0, 0});
			return Result;
		}
		Result.LogicalPath = NormalizedPath.Value;
		Result.ModuleName = ModuleNameFromLogicalPath(Result.LogicalPath);
		Result.ModuleId = MakeStableModuleId(Result.LogicalPath);
		if (!IsValidUtf8(Source.Contents))
		{
			AddDiagnostic(
				Result,
				"ASL-SOURCE-UTF8",
				"source is not valid UTF-8",
				{0, Source.Contents.size()});
			return Result;
		}

		Result.ProcessedSource = Source.Contents;
		const std::vector<FLexicalRange> LexicalRanges = ScanLexicalRanges(Source.Contents);
		const std::string Commentless = BlankCommentsPreservingLayout(Source.Contents);
		std::vector<FConditionalFrame> ConditionalStack;
		std::set<std::string> SeenImports;

		FByteOffset LineStart = 0;
		while (LineStart < Source.Contents.size())
		{
			const FByteOffset Newline = Source.Contents.find('\n', LineStart);
			const FByteOffset LineEnd =
				Newline == std::string::npos ? Source.Contents.size() : Newline;
			FByteOffset FirstContent = LineStart;
			while (FirstContent < LineEnd
				&& (Source.Contents[FirstContent] == ' '
					|| Source.Contents[FirstContent] == '\t'
					|| Source.Contents[FirstContent] == '\r'))
			{
				++FirstContent;
			}

			const bool bActive =
				ConditionalStack.empty() || ConditionalStack.back().bCurrentActive;
			bool bHandledDirective = false;
			if (FirstContent < LineEnd
				&& Source.Contents[FirstContent] == '#'
				&& IsCodeOffset(LexicalRanges, FirstContent))
			{
				bHandledDirective = true;
				std::string_view Directive(
					Source.Contents.data() + FirstContent + 1,
					LineEnd - FirstContent - 1);
				Directive = TrimLanguageSession(Directive);
				const std::size_t Separator = Directive.find_first_of(" \t");
				const std::string Name(Directive.substr(0, Separator));
				const std::string_view Argument =
					Separator == std::string_view::npos
						? std::string_view()
						: TrimLanguageSession(
							Directive.substr(Separator + 1));

				if (Name == "if" || Name == "ifdef" || Name == "ifndef")
				{
					const bool bParentActive = bActive;
					bool bCondition = false;
					bool bValid = true;
					std::string UnknownFlag;
					if (bParentActive)
					{
						std::string Expression(Argument);
						if (Name == "ifndef")
						{
							Expression.insert(Expression.begin(), '!');
						}
						bValid = EvaluateCondition(
							Expression,
							Config,
							bCondition,
							UnknownFlag);
						if (!bValid)
						{
							AddDiagnostic(
								Result,
								"ASL-PREPROCESS-UNKNOWN-FLAG",
								"invalid preprocessor condition: " + UnknownFlag,
								{FirstContent, LineEnd});
						}
					}
					ConditionalStack.push_back({
						bParentActive,
						bParentActive && bValid && bCondition,
						bParentActive && bValid && bCondition,
						false,
						FirstContent,
					});
				}
				else if (Name == "elif")
				{
					if (ConditionalStack.empty())
					{
						AddDiagnostic(
							Result,
							"ASL-PREPROCESS-UNMATCHED-ELIF",
							"invalid #elif, no matching #if found",
							{FirstContent, LineEnd});
					}
					else
					{
						FConditionalFrame& Frame = ConditionalStack.back();
						if (Frame.bSawElse)
						{
							AddDiagnostic(
								Result,
								"ASL-PREPROCESS-ELIF-AFTER-ELSE",
								"#elif cannot follow #else",
								{FirstContent, LineEnd});
							Frame.bCurrentActive = false;
						}
						else if (!Frame.bParentActive || Frame.bBranchTaken)
						{
							Frame.bCurrentActive = false;
						}
						else
						{
							bool bCondition = false;
							std::string UnknownFlag;
							const bool bValid = EvaluateCondition(
								Argument,
								Config,
								bCondition,
								UnknownFlag);
							if (!bValid)
							{
								AddDiagnostic(
									Result,
									"ASL-PREPROCESS-UNKNOWN-FLAG",
									"invalid preprocessor condition: " + UnknownFlag,
									{FirstContent, LineEnd});
							}
							Frame.bCurrentActive = bValid && bCondition;
							Frame.bBranchTaken = Frame.bCurrentActive;
						}
					}
				}
				else if (Name == "else")
				{
					if (ConditionalStack.empty())
					{
						AddDiagnostic(
							Result,
							"ASL-PREPROCESS-UNMATCHED-ELSE",
							"invalid #else, no matching #if found",
							{FirstContent, LineEnd});
					}
					else
					{
						FConditionalFrame& Frame = ConditionalStack.back();
						if (Frame.bSawElse)
						{
							AddDiagnostic(
								Result,
								"ASL-PREPROCESS-DUPLICATE-ELSE",
								"duplicate #else",
								{FirstContent, LineEnd});
						}
						Frame.bSawElse = true;
						Frame.bCurrentActive = Frame.bParentActive && !Frame.bBranchTaken;
						Frame.bBranchTaken = Frame.bBranchTaken || Frame.bCurrentActive;
					}
				}
				else if (Name == "endif")
				{
					if (ConditionalStack.empty())
					{
						AddDiagnostic(
							Result,
							"ASL-PREPROCESS-UNMATCHED-ENDIF",
							"invalid #endif, no matching #if found",
							{FirstContent, LineEnd});
					}
					else
					{
						ConditionalStack.pop_back();
					}
				}
				else if (Name == "include")
				{
					if (bActive)
					{
						AddDiagnostic(
							Result,
							"ASL-PREPROCESS-INCLUDE-UNSUPPORTED",
							"#include is unsupported; use import",
							{FirstContent, LineEnd});
					}
				}
				else
				{
					bHandledDirective = false;
				}
				if (bHandledDirective)
				{
					BlankRange(Result.ProcessedSource, LineStart, LineEnd);
				}
			}

			const bool bLineActive =
				ConditionalStack.empty() || ConditionalStack.back().bCurrentActive;
			if (!bHandledDirective && !bLineActive)
			{
				BlankRange(Result.ProcessedSource, LineStart, LineEnd);
			}
			else if (!bHandledDirective && bLineActive && FirstContent < LineEnd)
			{
				const std::string_view CommentlessLine(
					Commentless.data() + FirstContent,
					LineEnd - FirstContent);
				if (TrimLanguageSession(
						CommentlessLine).starts_with("import"))
				{
					std::string ImportedModule;
					if (!ParseImportModule(CommentlessLine, ImportedModule))
					{
						AddDiagnostic(
							Result,
							"ASL-PREPROCESS-IMPORT-SYNTAX",
							"invalid import statement",
							{FirstContent, LineEnd});
					}
					else if (SeenImports.insert(ImportedModule).second)
					{
						Result.Imports.push_back({
							std::move(ImportedModule),
							{FirstContent, LineEnd},
						});
					}
					BlankRange(Result.ProcessedSource, LineStart, LineEnd);
				}
			}

			if (Newline == std::string::npos)
			{
				break;
			}
			LineStart = Newline + 1;
		}

		for (const FConditionalFrame& Frame : ConditionalStack)
		{
			AddDiagnostic(
				Result,
				"ASL-PREPROCESS-UNCLOSED-CONDITION",
				"preprocessor condition was not closed; missing #endif",
				{Frame.DirectiveOffset, Frame.DirectiveOffset + 1});
		}

		const FDeclarationScanResult DeclarationResult =
			ScanDeclarations(
				{Result.LogicalPath, Source.Contents},
				Result.ProcessedSource);
		Result.Declarations = DeclarationResult.Declarations;
		for (const FSourceSpan Span : DeclarationResult.AnnotationSpans)
		{
			BlankRange(Result.ProcessedSource, Span.Begin, Span.End);
		}
		for (const FDeclarationDiagnostic& Diagnostic
			: DeclarationResult.Diagnostics)
		{
			AddDiagnostic(
				Result,
				Diagnostic.Code,
				Diagnostic.Message,
				Diagnostic.Span);
		}
		Result.bSuccess = Result.Diagnostics.empty();
		return Result;
	}

	FFrontendSession::FFrontendSession(FPreprocessConfig InConfig)
		: Config(std::move(InConfig))
	{
	}

	FPathResult FFrontendSession::AddSource(FSourceInput Source)
	{
		FPathResult Result = NormalizeLogicalPath(Source.LogicalPath);
		if (!Result.bSuccess)
		{
			return Result;
		}
		Source.LogicalPath = Result.Value;
		const std::string ModuleName = ModuleNameFromLogicalPath(Source.LogicalPath);
		if (SourcesByModule.contains(ModuleName))
		{
			return {false, {}, "duplicate module: " + ModuleName};
		}
		SourcesByModule.emplace(ModuleName, std::move(Source));
		Stage = EFrontendStage::SourcesAdded;
		return Result;
	}

	FFrontendSessionResult FFrontendSession::Process()
	{
		FFrontendSessionResult Result;
		Stage = SourcesByModule.empty()
			? EFrontendStage::Empty
			: EFrontendStage::ChunksProcessed;
		Result.Stage = Stage;

		std::map<std::string, FPreprocessResult> Preprocessed;
		for (const auto& [ModuleName, Source] : SourcesByModule)
		{
			FPreprocessResult ModuleResult = PreprocessSource(Source, Config);
			Result.Diagnostics.insert(
				Result.Diagnostics.end(),
				ModuleResult.Diagnostics.begin(),
				ModuleResult.Diagnostics.end());
			Preprocessed.emplace(ModuleName, std::move(ModuleResult));
		}
		if (!Result.Diagnostics.empty())
		{
			return Result;
		}
		for (const auto& [ModuleName, Module] : Preprocessed)
		{
			(void)ModuleName;
			for (const FImport& Import : Module.Imports)
			{
				if (!Preprocessed.contains(Import.ModuleName))
				{
					Result.Diagnostics.push_back({
						"ASL-PREPROCESS-IMPORT-MISSING",
						"imported module was not provided: "
							+ Import.ModuleName,
						Module.LogicalPath,
						Import.Span,
						EDiagnosticSeverity::Error,
					});
				}
			}
		}
		if (!Result.Diagnostics.empty())
		{
			return Result;
		}

		enum class EVisitState
		{
			Unvisited,
			Visiting,
			Visited,
		};
		std::map<std::string, EVisitState> VisitStates;
		std::vector<std::string> Stack;
		bool bCycle = false;
		std::function<void(const std::string&)> Visit =
			[&](const std::string& ModuleName)
		{
			if (bCycle || VisitStates[ModuleName] == EVisitState::Visited)
			{
				return;
			}
			if (VisitStates[ModuleName] == EVisitState::Visiting)
			{
				bCycle = true;
				std::ostringstream Chain;
				const auto CycleStart = std::find(Stack.begin(), Stack.end(), ModuleName);
				for (auto Iterator = CycleStart; Iterator != Stack.end(); ++Iterator)
				{
					if (Iterator != CycleStart)
					{
						Chain << " -> ";
					}
					Chain << *Iterator;
				}
				Chain << " -> " << ModuleName;
				Result.Diagnostics.push_back({
					"ASL-PREPROCESS-IMPORT-CYCLE",
					"circular import: " + Chain.str(),
					Preprocessed[ModuleName].LogicalPath,
					{0, 0},
					EDiagnosticSeverity::Error,
				});
				return;
			}

			VisitStates[ModuleName] = EVisitState::Visiting;
			Stack.push_back(ModuleName);
			for (const FImport& Import : Preprocessed[ModuleName].Imports)
			{
				if (Preprocessed.contains(Import.ModuleName))
				{
					Visit(Import.ModuleName);
				}
			}
			Stack.pop_back();
			VisitStates[ModuleName] = EVisitState::Visited;
			if (!bCycle)
			{
				const FPreprocessResult& Module = Preprocessed[ModuleName];
				FLanguageModule& Output = Result.Modules.emplace_back();
				Output.LogicalPath = Module.LogicalPath;
				Output.ModuleName = Module.ModuleName;
				Output.ModuleId = Module.ModuleId;
				Output.ProcessedSource = Module.ProcessedSource;
				Output.SourceMap = Module.SourceMap;
				Output.Declarations = Module.Declarations;
				for (const FImport& Import : Module.Imports)
				{
					Output.ImportedModules.push_back(Import.ModuleName);
				}
			}
		};

		for (const auto& [ModuleName, Module] : Preprocessed)
		{
			(void)Module;
			Visit(ModuleName);
		}
		if (bCycle)
		{
			Result.Modules.clear();
			return Result;
		}
		Stage = EFrontendStage::Completed;
		Result.Stage = Stage;
		Result.bSuccess = true;
		return Result;
	}

	EFrontendStage FFrontendSession::GetStage() const
	{
		return Stage;
	}
}
