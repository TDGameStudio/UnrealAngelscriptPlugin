#include "Compiler/AngelscriptStandaloneSourceGraph.h"

#include "Compiler/Frontend/AngelscriptStandaloneSource.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <system_error>
#include <utility>

namespace AngelscriptStandalone
{
	namespace
	{
		using namespace Frontend;

		bool IsWithin(
			const std::filesystem::path& Child,
			const std::filesystem::path& Parent)
		{
			auto ChildIterator = Child.begin();
			for (auto ParentIterator = Parent.begin();
				ParentIterator != Parent.end();
				++ParentIterator, ++ChildIterator)
			{
				if (ChildIterator == Child.end()
					|| *ChildIterator != *ParentIterator)
				{
					return false;
				}
			}
			return true;
		}

		void AddGraphDiagnostic(
			FStandaloneSourceGraphResult& Result,
			std::string Code,
			std::string Message,
			const FPreprocessResult& Module,
			const FSourceSpan Span = {})
		{
			Result.Diagnostics.push_back({
				std::move(Code),
				std::move(Message),
				Module.LogicalPath,
				Span,
				EDiagnosticSeverity::Error,
			});
		}
	}

	FStandaloneSourceCollectionResult CollectStandaloneSources(
		const std::vector<std::filesystem::path>& ScriptRoots)
	{
		using namespace Frontend;
		FStandaloneSourceCollectionResult Result;
		if (ScriptRoots.empty())
		{
			Result.Error = "at least one --script-root is required";
			return Result;
		}

		std::map<std::string, FSourceInput> SourcesByModule;
		for (const std::filesystem::path& InputRoot : ScriptRoots)
		{
			std::error_code ErrorCode;
			const std::filesystem::path Root =
				std::filesystem::weakly_canonical(InputRoot, ErrorCode);
			if (ErrorCode || !std::filesystem::is_directory(Root, ErrorCode))
			{
				Result.Error = "script root is unavailable: "
					+ InputRoot.generic_string();
				return Result;
			}

			std::filesystem::recursive_directory_iterator Iterator(
				Root,
				std::filesystem::directory_options::skip_permission_denied,
				ErrorCode);
			const std::filesystem::recursive_directory_iterator End;
			for (; !ErrorCode && Iterator != End; Iterator.increment(ErrorCode))
			{
				const std::filesystem::directory_entry& Entry = *Iterator;
				if (!Entry.is_regular_file(ErrorCode)
					|| ErrorCode
					|| Entry.path().extension() != ".as")
				{
					continue;
				}
				const std::filesystem::path Absolute =
					std::filesystem::weakly_canonical(Entry.path(), ErrorCode);
				if (ErrorCode || !IsWithin(Absolute, Root))
				{
					Result.Error =
						"source path escapes its script root";
					return Result;
				}
				const std::filesystem::path Relative =
					std::filesystem::relative(Absolute, Root, ErrorCode);
				if (ErrorCode)
				{
					Result.Error = "failed to derive source logical path";
					return Result;
				}
				const FPathResult Logical =
					NormalizeLogicalPath(Relative.generic_string());
				if (!Logical.bSuccess)
				{
					Result.Error = Logical.Error;
					return Result;
				}

				std::ifstream Input(Absolute, std::ios::binary);
				if (!Input)
				{
					Result.Error = "failed to read source: "
						+ Absolute.generic_string();
					return Result;
				}
				std::string Contents(
					(std::istreambuf_iterator<char>(Input)),
					std::istreambuf_iterator<char>());
				if (Contents.starts_with("\xef\xbb\xbf"))
				{
					Contents.erase(0, 3);
				}
				if (!IsValidUtf8(Contents))
				{
					Result.Error = "source is not valid UTF-8: "
						+ Logical.Value;
					return Result;
				}

				const std::string ModuleName =
					ModuleNameFromLogicalPath(Logical.Value);
				if (!SourcesByModule.emplace(
						ModuleName,
						FSourceInput{Logical.Value, std::move(Contents)})
						.second)
				{
					Result.Error =
						"duplicate logical module across script roots: "
						+ ModuleName;
					return Result;
				}
			}
			if (ErrorCode)
			{
				Result.Error = "failed while enumerating script root: "
					+ ErrorCode.message();
				return Result;
			}
		}

		Result.Sources.reserve(SourcesByModule.size());
		for (auto& [ModuleName, Source] : SourcesByModule)
		{
			(void)ModuleName;
			Result.Sources.emplace_back(std::move(Source));
		}
		Result.bSuccess = true;
		return Result;
	}

	FStandaloneSourceGraphResult BuildStandaloneSourceGraph(
		const FStandaloneSourceGraphRequest& Request)
	{
		using namespace Frontend;
		FStandaloneSourceGraphResult Result;
		if (Request.Entry.empty())
		{
			Result.Error = "an entry source or module is required";
			return Result;
		}

		std::map<std::string, FPreprocessResult> Modules;
		for (const FSourceInput& Source : Request.Sources)
		{
			FPreprocessResult Preprocessed =
				PreprocessSource(Source, Request.Config);
			if (Preprocessed.ModuleName.empty())
			{
				Result.Diagnostics.insert(
					Result.Diagnostics.end(),
					Preprocessed.Diagnostics.begin(),
					Preprocessed.Diagnostics.end());
				Result.Error = "source graph contains an invalid logical path";
				return Result;
			}
			if (!Modules.emplace(
					Preprocessed.ModuleName,
					std::move(Preprocessed)).second)
			{
				Result.Error = "duplicate logical module: "
					+ ModuleNameFromLogicalPath(Source.LogicalPath);
				return Result;
			}
		}

		const FPathResult NormalizedEntry =
			NormalizeLogicalPath(Request.Entry);
		Result.EntryModule = NormalizedEntry.bSuccess
			? ModuleNameFromLogicalPath(NormalizedEntry.Value)
			: Request.Entry;
		if (!Modules.contains(Result.EntryModule))
		{
			if (Modules.contains(Request.Entry))
			{
				Result.EntryModule = Request.Entry;
			}
			else
			{
				Result.Error = "entry module was not found: "
					+ Request.Entry;
				return Result;
			}
		}

		enum class EVisitState
		{
			Unvisited,
			Visiting,
			Visited,
		};
		std::map<std::string, EVisitState> States;
		std::vector<std::string> Stack;
		std::vector<std::string> OrderedModules;
		bool bFailed = false;
		std::function<void(const std::string&)> Visit =
			[&](const std::string& ModuleName)
			{
				if (bFailed || States[ModuleName] == EVisitState::Visited)
				{
					return;
				}
				if (States[ModuleName] == EVisitState::Visiting)
				{
					bFailed = true;
					const FPreprocessResult& Module = Modules.at(ModuleName);
					std::string Chain;
					const auto Start =
						std::find(Stack.begin(), Stack.end(), ModuleName);
					for (auto Iterator = Start; Iterator != Stack.end(); ++Iterator)
					{
						if (!Chain.empty())
						{
							Chain += " -> ";
						}
						Chain += *Iterator;
					}
					Chain += " -> " + ModuleName;
					AddGraphDiagnostic(
						Result,
						"ASL-PREPROCESS-IMPORT-CYCLE",
						"circular import: " + Chain,
						Module);
					return;
				}

				FPreprocessResult& Module = Modules.at(ModuleName);
				if (!Module.bSuccess)
				{
					bFailed = true;
					Result.Diagnostics.insert(
						Result.Diagnostics.end(),
						Module.Diagnostics.begin(),
						Module.Diagnostics.end());
					return;
				}
				States[ModuleName] = EVisitState::Visiting;
				Stack.push_back(ModuleName);
				for (const FImport& Import : Module.Imports)
				{
					if (!Modules.contains(Import.ModuleName))
					{
						bFailed = true;
						AddGraphDiagnostic(
							Result,
							"ASL-PREPROCESS-IMPORT-MISSING",
							"imported module was not provided: "
								+ Import.ModuleName,
							Module,
							Import.Span);
						break;
					}
					Visit(Import.ModuleName);
				}
				Stack.pop_back();
				States[ModuleName] = EVisitState::Visited;
				if (!bFailed)
				{
					OrderedModules.push_back(ModuleName);
				}
			};
		Visit(Result.EntryModule);
		if (bFailed)
		{
			Result.Error = "source closure could not be constructed";
			return Result;
		}

		for (const std::string& ModuleName : OrderedModules)
		{
			const FPreprocessResult& Module = Modules.at(ModuleName);
			FLanguageModule& Output = Result.Modules.emplace_back();
			Output.LogicalPath = Module.LogicalPath;
			Output.ModuleName = Module.ModuleName;
			Output.ModuleId = Module.ModuleId;
			Output.ProcessedSource = Module.ProcessedSource;
			Output.Declarations = Module.Declarations;
			for (const FImport& Import : Module.Imports)
			{
				Output.ImportedModules.push_back(Import.ModuleName);
			}
		}
		Result.bSuccess = true;
		return Result;
	}
}
