#include "CLI/AngelscriptStandaloneArguments.h"
#include "Compiler/AngelscriptStandaloneArtifact.h"
#include "Compiler/AngelscriptStandaloneNativeCompiler.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
	bool Require(bool Condition, const char* What)
	{
		if (!Condition)
		{
			std::cerr << "FAILED: " << What << '\n';
		}
		return Condition;
	}

	std::filesystem::path MakeTemporaryDirectory()
	{
		const auto Suffix = std::chrono::steady_clock::now().time_since_epoch().count();
		const auto Directory = std::filesystem::temp_directory_path()
			/ ("angelscript-standalone-cli-" + std::to_string(Suffix));
		std::filesystem::create_directories(Directory);
		return Directory;
	}
}

int main()
{
	using namespace AngelscriptStandalone;

	bool Passed = true;
	{
		const char* Arguments[] = {
			"as-standalone",
			"compile",
			"--dialect", "native",
			"--script-root", "scripts",
			"--entry", "main.as",
			"--output", "artifacts",
			"--diagnostics", "json",
			"--emit-bytecode",
		};
		const FArgumentParseResult Parsed = ParseArguments(
			static_cast<int>(std::size(Arguments)),
			const_cast<char**>(Arguments));
		Passed &= Require(Parsed.bSuccess, "parse valid native compile command");
		Passed &= Require(Parsed.Options.Command == ECommand::Compile, "compile command");
		Passed &= Require(Parsed.Options.Dialect == EDialect::Native, "native dialect");
		Passed &= Require(Parsed.Options.ScriptRoots.size() == 1, "one script root");
		Passed &= Require(Parsed.Options.bEmitByteCode, "bytecode enabled");
	}

	{
		const char* Arguments[] = {
			"as-standalone",
			"compile",
			"--dialect", "ue",
			"--script-root", "scripts",
			"--entry", "main.as",
			"--bundle", "project-contract",
			"--allow-ue-required",
			"--strict-resources",
			"--emit-bytecode",
		};
		const FArgumentParseResult Parsed = ParseArguments(
			static_cast<int>(std::size(Arguments)),
			const_cast<char**>(Arguments));
		Passed &= Require(
			Parsed.bSuccess,
			"parse valid UE-validation compile command");
		Passed &= Require(
			Parsed.Options.Dialect == EDialect::Unreal,
			"UE dialect");
		Passed &= Require(
			Parsed.Options.Bundle == "project-contract",
			"explicit UE bundle");
		Passed &= Require(
			Parsed.Options.bAllowUERequired
				&& Parsed.Options.bStrictResources,
			"UE validation policies");
	}

	{
		const char* Arguments[] = {
			"as-standalone",
			"run",
			"--bundle", "ue-contract",
			"--script-root", "scripts",
			"--entry", "main.as",
		};
		const FArgumentParseResult Parsed = ParseArguments(
			static_cast<int>(std::size(Arguments)),
			const_cast<char**>(Arguments));
		Passed &= Require(!Parsed.bSuccess, "run rejects UE bundle option");
		Passed &= Require(Parsed.ExitCode == 2, "usage error exit code");
	}

	const std::filesystem::path TemporaryDirectory = MakeTemporaryDirectory();
	const std::filesystem::path ScriptRoot = TemporaryDirectory / "scripts";
	const std::filesystem::path OutputRoot = TemporaryDirectory / "output";
	std::filesystem::create_directories(ScriptRoot);
	{
		std::ofstream Source(ScriptRoot / "main.as", std::ios::binary);
		Source << "int main(const array<string> args) { return args.length() == 0 ? 23 : -1; }\n";
	}

	FCompileRequest Request;
	Request.ScriptRoots.push_back(ScriptRoot);
	Request.Entry = "main.as";
	Request.bEmitByteCode = true;

	FNativeCompiler Compiler;
	const FCompileResult First = Compiler.Compile(Request);
	const FCompileResult Second = Compiler.Compile(Request);
	Passed &= Require(First.bSuccess, "compile native entry");
	Passed &= Require(Second.bSuccess, "compile native entry twice");
	Passed &= Require(!First.ByteCode.empty(), "native bytecode emitted");
	Passed &= Require(First.ByteCode == Second.ByteCode, "native bytecode deterministic");
	Passed &= Require(First.ProfileHash == Second.ProfileHash, "native profile identity deterministic");
	Passed &= Require(First.InputHash == Second.InputHash, "native input identity deterministic");

	FArtifactWriteRequest ArtifactRequest;
	ArtifactRequest.OutputDirectory = OutputRoot;
	ArtifactRequest.CompileResult = &First;
	const FArtifactWriteResult ArtifactResult = WriteCompileArtifacts(ArtifactRequest);
	Passed &= Require(ArtifactResult.bSuccess, "write native compile artifacts");
	Passed &= Require(std::filesystem::exists(OutputRoot / "result.json"), "result.json exists");
	Passed &= Require(std::filesystem::exists(OutputRoot / "diagnostics.jsonl"), "diagnostics.jsonl exists");
	Passed &= Require(
		std::filesystem::exists(OutputRoot / "modules" / (First.ModuleId + ".asbc")),
		"module bytecode exists");

	std::error_code CleanupError;
	std::filesystem::remove_all(TemporaryDirectory, CleanupError);
	return Passed ? 0 : 1;
}
