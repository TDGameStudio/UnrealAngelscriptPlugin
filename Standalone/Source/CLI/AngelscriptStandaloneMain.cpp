#include "CLI/AngelscriptStandaloneArguments.h"
#include "Compiler/AngelscriptStandaloneArtifact.h"
#include "Compiler/AngelscriptStandaloneNativeCompiler.h"
#include "Compiler/AngelscriptStandaloneUECompiler.h"
#include "Compiler/AngelscriptValidationArtifact.h"
#include "Runtime/AngelscriptStandaloneRunner.h"
#include "Support/AngelscriptStandaloneJson.h"

#include "UnrealAngelscriptVersion.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace AngelscriptStandalone
{
	namespace
	{
		std::filesystem::path ResolveOutputDirectory(const std::filesystem::path& OutputDirectory)
		{
			std::error_code Error;
			std::filesystem::path Resolved = std::filesystem::absolute(OutputDirectory, Error);
			return Error ? OutputDirectory.lexically_normal() : Resolved.lexically_normal();
		}

		void PrintDiagnostic(const FDiagnostic& Diagnostic, EDiagnosticFormat Format)
		{
			if (Format == EDiagnosticFormat::Json)
			{
				std::cerr
					<< "{\"code\":" << EscapeJsonString(Diagnostic.Code)
					<< ",\"column\":" << Diagnostic.Column
					<< ",\"message\":" << EscapeJsonString(Diagnostic.Message)
					<< ",\"row\":" << Diagnostic.Row
					<< ",\"section\":" << EscapeJsonString(Diagnostic.Section)
					<< ",\"severity\":" << EscapeJsonString(ToString(Diagnostic.Severity))
					<< "}\n";
				return;
			}

			if (!Diagnostic.Section.empty())
			{
				std::cerr
					<< Diagnostic.Section << '(' << Diagnostic.Row << ',' << Diagnostic.Column << "): ";
			}
			std::cerr
				<< ToString(Diagnostic.Severity);
			if (!Diagnostic.Code.empty())
			{
				std::cerr << ' ' << Diagnostic.Code;
			}
			std::cerr << ": " << Diagnostic.Message << '\n';
		}

		void PrintDiagnostics(
			const std::vector<FDiagnostic>& Diagnostics,
			EDiagnosticFormat Format)
		{
			for (const FDiagnostic& Diagnostic : Diagnostics)
			{
				PrintDiagnostic(Diagnostic, Format);
			}
		}

		int CompileNative(const FCommandLineOptions& Options)
		{
			FCompileRequest Request;
			Request.ScriptRoots = Options.ScriptRoots;
			Request.Entry = Options.Entry;
			Request.bEmitByteCode = Options.bEmitByteCode;

			FNativeCompiler Compiler;
			const FCompileResult CompileResult = Compiler.Compile(Request);
			PrintDiagnostics(CompileResult.Diagnostics, Options.DiagnosticFormat);

			if (CompileResult.ModuleId.empty())
			{
				std::cerr
					<< (CompileResult.Error.empty()
						? "native compilation failed before source resolution"
						: CompileResult.Error)
					<< '\n';
				return 2;
			}

			const std::filesystem::path OutputDirectory =
				ResolveOutputDirectory(Options.OutputDirectory);
			FArtifactWriteRequest ArtifactRequest;
			ArtifactRequest.OutputDirectory = OutputDirectory;
			ArtifactRequest.CompileResult = &CompileResult;
			const FArtifactWriteResult ArtifactResult = WriteCompileArtifacts(ArtifactRequest);
			if (!ArtifactResult.bSuccess)
			{
				std::cerr << ArtifactResult.Error << '\n';
				return 2;
			}

			std::cerr << "artifacts: " << OutputDirectory.generic_string() << '\n';
			if (!CompileResult.bSuccess && !CompileResult.Error.empty())
			{
				std::cerr << CompileResult.Error << '\n';
			}
			return CompileResult.bSuccess ? 0 : 1;
		}

		int CompileUnreal(
			const FCommandLineOptions& Options,
			const std::filesystem::path& ExecutablePath)
		{
			FUECompileRequest Request;
			Request.ScriptRoots = Options.ScriptRoots;
			Request.Entry = Options.Entry;
			if (!Options.Bundle.empty())
			{
				Request.ExplicitBundle = Options.Bundle;
			}
			Request.PackagedDefaultBundle =
				ExecutablePath.parent_path()
				/ "contracts"
				/ "default-engine";
			Request.bEmitByteCode = Options.bEmitByteCode;
			Request.bStrictResources = Options.bStrictResources;
			Request.bAllowUERequired = Options.bAllowUERequired;

			const FUECompiler Compiler;
			const FUECompileResult CompileResult =
				Compiler.Compile(Request);
			PrintDiagnostics(
				CompileResult.Diagnostics,
				Options.DiagnosticFormat);
			if (CompileResult.bInfrastructureFailure)
			{
				std::cerr
					<< (CompileResult.Error.empty()
						? "UE-validation infrastructure failed"
						: CompileResult.Error)
					<< '\n';
				return 2;
			}
			if (CompileResult.ModuleId.empty())
			{
				std::cerr
					<< (CompileResult.Error.empty()
						? "UE-validation failed before source resolution"
						: CompileResult.Error)
					<< '\n';
				return 1;
			}

			const std::filesystem::path OutputDirectory =
				ResolveOutputDirectory(Options.OutputDirectory);
			const FValidationArtifactWriteResult ArtifactResult =
				WriteValidationArtifacts({
					OutputDirectory,
					&CompileResult,
				});
			if (!ArtifactResult.bSuccess)
			{
				std::cerr << ArtifactResult.Error << '\n';
				return 2;
			}
			std::cerr
				<< "artifacts: "
				<< OutputDirectory.generic_string()
				<< '\n';
			if (!CompileResult.bSuccess
				&& !CompileResult.Error.empty())
			{
				std::cerr << CompileResult.Error << '\n';
			}
			return CompileResult.bSuccess ? 0 : 1;
		}

		int RunNative(const FCommandLineOptions& Options)
		{
			FRunRequest Request;
			Request.ScriptRoots = Options.ScriptRoots;
			Request.Entry = Options.Entry;
			Request.Arguments = Options.ScriptArguments;
			Request.TimeoutMilliseconds = Options.TimeoutMilliseconds;
			Request.MemoryLimitBytes =
				static_cast<std::uint64_t>(Options.MemoryLimitMegabytes) * 1024ull * 1024ull;

			const FRunResult RunResult = RunNativeScript(Request);
			if (!RunResult.StandardOutput.empty())
			{
				std::cout << RunResult.StandardOutput;
			}
			PrintDiagnostics(RunResult.Diagnostics, Options.DiagnosticFormat);

			if (RunResult.ModuleId.empty())
			{
				std::cerr
					<< (RunResult.Error.empty()
						? "native run failed before source resolution"
						: RunResult.Error)
					<< '\n';
				return 2;
			}

			const std::filesystem::path OutputDirectory =
				ResolveOutputDirectory(Options.OutputDirectory);
			FRunArtifactWriteRequest ArtifactRequest;
			ArtifactRequest.OutputDirectory = OutputDirectory;
			ArtifactRequest.RunResult = &RunResult;
			const FArtifactWriteResult ArtifactResult = WriteRunArtifacts(ArtifactRequest);
			if (!ArtifactResult.bSuccess)
			{
				std::cerr << ArtifactResult.Error << '\n';
				return 2;
			}

			std::cerr << "artifacts: " << OutputDirectory.generic_string() << '\n';
			if (!RunResult.Error.empty() && RunResult.Diagnostics.empty())
			{
				std::cerr << RunResult.Error << '\n';
			}
			return RunResult.ExitCode;
		}

		std::string GetVersionText()
		{
			return
				std::string(UNREAL_ANGELSCRIPT_PRODUCT_VERSION_STRING) + "\n"
				+ UNREAL_ANGELSCRIPT_UPSTREAM_LINEAGE_STRING + "\n"
				+ "as-standalone compiler 1.0\n"
				+ "native-runtime profile "
				+ FNativeCompiler::GetProfileHash() + "\n"
				+ "ue-validation profile 1.0 (compile-only)\n"
				+ "result schema angelscript-standalone-result/1.0\n";
		}
	}
}

int main(int ArgumentCount, char** Arguments)
{
	using namespace AngelscriptStandalone;

	const FArgumentParseResult Parsed = ParseArguments(ArgumentCount, Arguments);
	if (!Parsed.bSuccess)
	{
		if (!Parsed.Error.empty())
		{
			std::cerr << Parsed.Error << '\n';
		}
		std::cerr << GetHelpText();
		return Parsed.ExitCode;
	}

	switch (Parsed.Options.Command)
	{
	case ECommand::Help:
		std::cout << GetHelpText();
		return 0;
	case ECommand::Version:
		std::cout << GetVersionText();
		return 0;
	case ECommand::Compile:
		if (Parsed.Options.Dialect == EDialect::Unreal)
		{
			std::error_code ErrorCode;
			std::filesystem::path Executable =
				std::filesystem::absolute(Arguments[0], ErrorCode);
			if (ErrorCode)
			{
				Executable = Arguments[0];
			}
			return CompileUnreal(
				Parsed.Options,
				Executable.lexically_normal());
		}
		return CompileNative(Parsed.Options);
	case ECommand::Run:
		return RunNative(Parsed.Options);
	default:
		std::cerr << "a command is required\n";
		return 2;
	}
}
