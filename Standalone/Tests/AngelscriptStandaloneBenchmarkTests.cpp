#include "Compiler/AngelscriptStandaloneNativeCompiler.h"
#include "Compiler/AngelscriptStandaloneUECompiler.h"
#include "Contract/AngelscriptOfflineBundleLoader.h"
#include "Runtime/AngelscriptStandaloneRunner.h"

#include "angelscript.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	namespace fs = std::filesystem;
	using namespace AngelscriptStandalone;
	using FClock = std::chrono::steady_clock;

	enum class EComparison
	{
		Pass,
		Regression,
		Incomparable,
	};

	EComparison CompareMedian(
		const std::string_view BaselineEnvironment,
		const double Baseline,
		const std::string_view CurrentEnvironment,
		const double Current)
	{
		if (BaselineEnvironment != CurrentEnvironment)
		{
			return EComparison::Incomparable;
		}
		return Current > Baseline * 1.20
			? EComparison::Regression
			: EComparison::Pass;
	}

	bool Require(const bool bCondition, const std::string_view Message)
	{
		if (!bCondition)
		{
			std::cerr << Message << '\n';
		}
		return bCondition;
	}

	template<typename F>
	double MeasureMilliseconds(F&& Function)
	{
		const auto Begin = FClock::now();
		Function();
		const auto End = FClock::now();
		return std::chrono::duration<double, std::milli>(
			End - Begin).count();
	}

	double Median(std::vector<double> Values)
	{
		std::sort(Values.begin(), Values.end());
		const std::size_t Middle = Values.size() / 2;
		return Values.size() % 2 == 0
			? (Values[Middle - 1] + Values[Middle]) / 2.0
			: Values[Middle];
	}

	std::string EscapeJson(const std::string_view Value)
	{
		std::string Result;
		for (const char Character : Value)
		{
			if (Character == '\\' || Character == '"')
			{
				Result.push_back('\\');
			}
			Result.push_back(Character);
		}
		return Result;
	}

	struct FMetric
	{
		std::string Name;
		std::vector<double> Samples;
	};
}

int main(int ArgumentCount, char** Arguments)
{
	const fs::path OutputRoot =
		ArgumentCount >= 3
			&& std::string_view(Arguments[1]) == "--output"
		? fs::path(Arguments[2])
		: fs::path("benchmark-output");
	const fs::path CorpusRoot =
		fs::path(ANGELSCRIPT_STANDALONE_CORPUS_ROOT);
	const fs::path BundleRoot =
		fs::path(ANGELSCRIPT_OFFLINE_FIXTURE_ROOT)
			/ "default-engine";
	constexpr int SampleCount = 11;

	bool bPassed = true;
	bPassed &= Require(
		CompareMedian("same", 100.0, "same", 120.0)
			== EComparison::Pass,
		"20 percent benchmark boundary did not pass");
	bPassed &= Require(
		CompareMedian("same", 100.0, "same", 120.01)
			== EComparison::Regression,
		"greater-than-20-percent regression did not fail");
	bPassed &= Require(
		CompareMedian("first", 100.0, "second", 200.0)
			== EComparison::Incomparable,
		"mismatched benchmark environments were compared");

	std::vector<FMetric> Metrics = {
		{"cold-engine-create-shutdown-ms", {}},
		{"native-compile-ms", {}},
		{"native-compile-run-ms", {}},
		{"bundle-load-index-ms", {}},
		{"ue-core-analysis-ms", {}},
		{"template-analysis-ms", {}},
		{"resource-analysis-ms", {}},
	};
	std::uint64_t PeakMemoryBytes = 0;
	std::string BundleIdentity;
	for (int Sample = 0; Sample < SampleCount; ++Sample)
	{
		Metrics[0].Samples.push_back(MeasureMilliseconds([&]
		{
			asIScriptEngine* Engine = asCreateScriptEngine();
			if (Engine != nullptr)
			{
				Engine->ShutDownAndRelease();
			}
			else
			{
				bPassed = false;
			}
		}));

		Metrics[1].Samples.push_back(MeasureMilliseconds([&]
		{
			FCompileRequest Request;
			Request.ScriptRoots = {CorpusRoot / "Native"};
			Request.Entry = "basic.as";
			Request.bEmitByteCode = true;
			const FCompileResult Result =
				FNativeCompiler().Compile(Request);
			bPassed &= Result.bSuccess
				&& !Result.ByteCode.empty();
		}));

		Metrics[2].Samples.push_back(MeasureMilliseconds([&]
		{
			FRunRequest Request;
			Request.ScriptRoots = {CorpusRoot / "Native"};
			Request.Entry = "basic.as";
			const FRunResult Result = RunNativeScript(Request);
			bPassed &= Result.ExitCode == 0
				&& Result.AllocatedBytesAfterShutdown == 0;
			PeakMemoryBytes = std::max(
				PeakMemoryBytes,
				Result.PeakAllocatedBytes);
		}));

		Metrics[3].Samples.push_back(MeasureMilliseconds([&]
		{
			FOfflineBundleLoadOptions Options;
			Options.Compatibility.ExpectedForkVersion =
				"2.33+selective-2.38";
			Options.Compatibility.ExpectedCompilerContractVersion =
				"ue-as-standalone-v1";
			const FOfflineBundleLoadResult Result =
				LoadSelectedOfflineBundle(
					BundleRoot,
					{},
					Options);
			bPassed &= Result.bSuccess;
			if (Result.bSuccess)
			{
				BundleIdentity =
					Result.Bundle.Manifest.BundleIdentity;
			}
		}));

		Metrics[4].Samples.push_back(MeasureMilliseconds([&]
		{
			FUECompileRequest Request;
			Request.ScriptRoots = {CorpusRoot / "UECore"};
			Request.Entry = "declarations.as";
			Request.ExplicitBundle = BundleRoot;
			Request.bEmitByteCode = true;
			const FUECompileResult Result =
				FUECompiler().Compile(Request);
			bPassed &= Result.bSuccess
				&& !Result.ByteCode.empty();
		}));

		Metrics[5].Samples.push_back(MeasureMilliseconds([&]
		{
			FUECompileRequest Request;
			Request.ScriptRoots = {CorpusRoot / "Templates"};
			Request.Entry = "array-without-adapter.as";
			Request.ExplicitBundle = BundleRoot;
			const FUECompileResult Result =
				FUECompiler().Compile(Request);
			bPassed &= !Result.bSuccess
				&& !Result.bInfrastructureFailure;
		}));

		Metrics[6].Samples.push_back(MeasureMilliseconds([&]
		{
			FUECompileRequest Request;
			Request.ScriptRoots = {CorpusRoot / "Resources"};
			Request.Entry = "soft-path-without-symbol.as";
			Request.ExplicitBundle = BundleRoot;
			const FUECompileResult Result =
				FUECompiler().Compile(Request);
			bPassed &= !Result.bSuccess
				&& !Result.bInfrastructureFailure;
		}));
	}

	std::error_code Error;
	fs::create_directories(OutputRoot, Error);
	bPassed &= Require(!Error, "could not create benchmark output");
	if (Error)
	{
		return 1;
	}

#if defined(_MSC_FULL_VER)
	const std::string Compiler =
		"msvc-" + std::to_string(_MSC_FULL_VER);
#else
	const std::string Compiler = "unknown-compiler";
#endif
#if defined(NDEBUG)
	const std::string Configuration = "release";
#else
	const std::string Configuration = "debug";
#endif
	const std::string EnvironmentIdentity =
		"win64|" + Compiler + "|" + Configuration
		+ "|" + FNativeCompiler::GetProfileHash()
		+ "|" + FUECompiler::GetProfileHash();

	std::ofstream Csv(
		OutputRoot / "benchmark-results.csv",
		std::ios::binary | std::ios::trunc);
	Csv << "metric,sample_count,median\n";
	for (const FMetric& Metric : Metrics)
	{
		Csv << Metric.Name << ',' << Metric.Samples.size()
			<< ',' << std::fixed << std::setprecision(6)
			<< Median(Metric.Samples) << '\n';
	}
	Csv << "native-peak-memory-bytes," << SampleCount
		<< ',' << PeakMemoryBytes << '\n';

	std::ofstream Json(
		OutputRoot / "benchmark-results.json",
		std::ios::binary | std::ios::trunc);
	Json
		<< "{\"bundleIdentity\":\""
		<< EscapeJson(BundleIdentity)
		<< "\",\"commit\":\""
		<< ANGELSCRIPT_STANDALONE_GIT_COMMIT
		<< "\",\"configuration\":\""
		<< Configuration
		<< "\",\"environmentIdentity\":\""
		<< EscapeJson(EnvironmentIdentity)
		<< "\",\"metrics\":[";
	for (std::size_t Index = 0; Index < Metrics.size(); ++Index)
	{
		if (Index != 0)
		{
			Json << ',';
		}
		Json
			<< "{\"median\":"
			<< std::fixed << std::setprecision(6)
			<< Median(Metrics[Index].Samples)
			<< ",\"name\":\""
			<< Metrics[Index].Name
			<< "\",\"sampleCount\":"
			<< Metrics[Index].Samples.size()
			<< '}';
	}
	Json
		<< "],\"nativePeakMemoryBytes\":"
		<< PeakMemoryBytes
		<< ",\"regressionThresholdPercent\":20"
		<< ",\"schema\":\"angelscript-standalone-benchmark/1.0\"}\n";

	return bPassed && Csv.good() && Json.good() ? 0 : 1;
}
