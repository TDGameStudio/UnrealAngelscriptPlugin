#include "CLI/AngelscriptStandaloneArguments.h"

#include <charconv>
#include <string_view>

namespace AngelscriptStandalone
{
	namespace
	{
		bool ParsePositiveInteger(std::string_view Text, int& OutValue)
		{
			int Value = 0;
			const auto Result = std::from_chars(Text.data(), Text.data() + Text.size(), Value);
			if (Result.ec != std::errc() || Result.ptr != Text.data() + Text.size() || Value <= 0)
			{
				return false;
			}
			OutValue = Value;
			return true;
		}

		FArgumentParseResult Fail(std::string Error)
		{
			FArgumentParseResult Result;
			Result.Error = std::move(Error);
			return Result;
		}
	}

	FArgumentParseResult ParseArguments(int ArgumentCount, char** Arguments)
	{
		if (ArgumentCount < 2 || Arguments == nullptr)
		{
			return Fail("a command is required");
		}

		FArgumentParseResult Result;
		const std::string_view Command = Arguments[1];
		if (Command == "--help" || Command == "-h" || Command == "help")
		{
			Result.Options.Command = ECommand::Help;
			Result.bSuccess = true;
			Result.ExitCode = 0;
			return Result;
		}
		if (Command == "--version" || Command == "version")
		{
			Result.Options.Command = ECommand::Version;
			Result.bSuccess = true;
			Result.ExitCode = 0;
			return Result;
		}
		if (Command == "compile")
		{
			Result.Options.Command = ECommand::Compile;
		}
		else if (Command == "run")
		{
			Result.Options.Command = ECommand::Run;
		}
		else
		{
			return Fail("unknown command: " + std::string(Command));
		}

		bool SawDialect = false;
		for (int Index = 2; Index < ArgumentCount; ++Index)
		{
			const std::string_view Option = Arguments[Index];
			if (Option == "--" && Result.Options.Command == ECommand::Run)
			{
				for (++Index; Index < ArgumentCount; ++Index)
				{
					Result.Options.ScriptArguments.emplace_back(Arguments[Index]);
				}
				break;
			}

			auto RequireValue = [&](std::string_view Name) -> const char*
			{
				if (Index + 1 >= ArgumentCount)
				{
					Result.Error = std::string(Name) + " requires a value";
					return nullptr;
				}
				return Arguments[++Index];
			};

			if (Option == "--dialect")
			{
				const char* Value = RequireValue(Option);
				if (Value == nullptr)
				{
					return Result;
				}
				SawDialect = true;
				if (std::string_view(Value) == "native")
				{
					Result.Options.Dialect = EDialect::Native;
				}
				else if (std::string_view(Value) == "ue")
				{
					Result.Options.Dialect = EDialect::Unreal;
				}
				else
				{
					return Fail("--dialect must be native or ue");
				}
			}
			else if (Option == "--script-root")
			{
				const char* Value = RequireValue(Option);
				if (Value == nullptr)
				{
					return Result;
				}
				Result.Options.ScriptRoots.emplace_back(Value);
			}
			else if (Option == "--entry")
			{
				const char* Value = RequireValue(Option);
				if (Value == nullptr)
				{
					return Result;
				}
				Result.Options.Entry = Value;
			}
			else if (Option == "--bundle")
			{
				const char* Value = RequireValue(Option);
				if (Value == nullptr)
				{
					return Result;
				}
				Result.Options.Bundle = Value;
			}
			else if (Option == "--output")
			{
				const char* Value = RequireValue(Option);
				if (Value == nullptr)
				{
					return Result;
				}
				Result.Options.OutputDirectory = Value;
			}
			else if (Option == "--diagnostics")
			{
				const char* Value = RequireValue(Option);
				if (Value == nullptr)
				{
					return Result;
				}
				if (std::string_view(Value) == "text")
				{
					Result.Options.DiagnosticFormat = EDiagnosticFormat::Text;
				}
				else if (std::string_view(Value) == "json")
				{
					Result.Options.DiagnosticFormat = EDiagnosticFormat::Json;
				}
				else
				{
					return Fail("--diagnostics must be text or json");
				}
			}
			else if (Option == "--emit-bytecode")
			{
				Result.Options.bEmitByteCode = true;
			}
			else if (Option == "--strict-resources")
			{
				Result.Options.bStrictResources = true;
			}
			else if (Option == "--allow-ue-required")
			{
				Result.Options.bAllowUERequired = true;
			}
			else if (Option == "--timeout-ms")
			{
				const char* Value = RequireValue(Option);
				if (Value == nullptr
					|| !ParsePositiveInteger(Value, Result.Options.TimeoutMilliseconds))
				{
					return Fail("--timeout-ms requires a positive integer");
				}
			}
			else if (Option == "--memory-limit-mb")
			{
				const char* Value = RequireValue(Option);
				if (Value == nullptr
					|| !ParsePositiveInteger(Value, Result.Options.MemoryLimitMegabytes))
				{
					return Fail("--memory-limit-mb requires a positive integer");
				}
			}
			else
			{
				return Fail("unknown option: " + std::string(Option));
			}
		}

		if (Result.Options.ScriptRoots.empty() || Result.Options.Entry.empty())
		{
			return Fail("--script-root and --entry are required");
		}
		if (Result.Options.Command == ECommand::Compile && !SawDialect)
		{
			return Fail("compile requires --dialect native or ue");
		}
		if (Result.Options.Command == ECommand::Run)
		{
			if (SawDialect && Result.Options.Dialect != EDialect::Native)
			{
				return Fail("run supports only the native profile");
			}
			if (!Result.Options.Bundle.empty()
				|| Result.Options.bStrictResources
				|| Result.Options.bAllowUERequired)
			{
				return Fail("run does not accept UE-validation options");
			}
			Result.Options.Dialect = EDialect::Native;
		}
		if (Result.Options.Dialect == EDialect::Native
			&& (!Result.Options.Bundle.empty()
				|| Result.Options.bStrictResources
				|| Result.Options.bAllowUERequired))
		{
			return Fail("native compilation does not accept UE-validation options");
		}

		Result.bSuccess = true;
		Result.ExitCode = 0;
		return Result;
	}

	std::string GetHelpText()
	{
		return
			"as-standalone compile --dialect native|ue --script-root <directory> "
			"--entry <file> [--bundle <directory>] --output <directory> "
			"--diagnostics text|json [--emit-bytecode] [--strict-resources] "
			"[--allow-ue-required]\n"
			"as-standalone run --script-root <directory> --entry <file> "
			"[--timeout-ms 5000] [--memory-limit-mb 256] "
			"[--diagnostics text|json] [-- <arguments...>]\n";
	}
}
