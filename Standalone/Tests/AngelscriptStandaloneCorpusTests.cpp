#include "Compiler/AngelscriptStandaloneUECompiler.h"
#include "Compiler/AngelscriptStandaloneDifferentialResult.h"
#include "Runtime/AngelscriptStandaloneRunner.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <string_view>

namespace
{
	namespace fs = std::filesystem;
	using namespace AngelscriptStandalone;

	bool Require(
		const bool bCondition,
		const std::string_view Case,
		const std::string_view Message)
	{
		if (!bCondition)
		{
			std::cerr << Case << ": " << Message << '\n';
		}
		return bCondition;
	}

	std::string ReadText(const fs::path& Path)
	{
		std::ifstream Input(Path, std::ios::binary);
		return {
			std::istreambuf_iterator<char>(Input),
			std::istreambuf_iterator<char>(),
		};
	}

	bool IsKnownClassification(const std::string_view Value)
	{
		return Value == "supported"
			|| Value == "compile-shim"
			|| Value == "ue-required"
			|| Value == "unsupported"
			|| Value == "intentional-host-difference";
	}
}

int main()
{
	FDifferentialResult UnrealEvidence;
	UnrealEvidence.CaseId = "differential.contract";
	UnrealEvidence.Host = "unreal";
	UnrealEvidence.Profile = "ue-validation";
	UnrealEvidence.CompileStatus = "complete";
	UnrealEvidence.Classification = "supported";
	UnrealEvidence.bByteCodeCompleted = true;
	UnrealEvidence.ResolvedStableSymbolIds = {
		std::string(64, 'b'),
		std::string(64, 'a'),
	};
	UnrealEvidence.Diagnostics = {
		{"AS200", "type", "Game/Test.as", 20, 24, "error"},
		{"AS100", "syntax", "Game/Test.as", 4, 5, "warning"},
	};
	FDifferentialResult StandaloneEvidence = UnrealEvidence;
	StandaloneEvidence.Host = "standalone";
	std::reverse(
		StandaloneEvidence.ResolvedStableSymbolIds.begin(),
		StandaloneEvidence.ResolvedStableSymbolIds.end());
	std::reverse(
		StandaloneEvidence.Diagnostics.begin(),
		StandaloneEvidence.Diagnostics.end());
	const FDifferentialComparison Equivalent =
		CompareDifferentialResults(
			UnrealEvidence,
			StandaloneEvidence);
	bool bPassed = Require(
		Equivalent.bEquivalent
			&& SerializeDifferentialResult(UnrealEvidence).find(
				"elapsed") == std::string::npos
			&& SerializeDifferentialResult(UnrealEvidence).find(
				"bytecodeBytes") == std::string::npos,
		"differential-result",
		"host/order normalization failed or forbidden evidence "
			"entered the differential schema");
	StandaloneEvidence.Diagnostics[0].Begin = 99;
	const FDifferentialComparison Different =
		CompareDifferentialResults(
			UnrealEvidence,
			StandaloneEvidence);
	bPassed &= Require(
		!Different.bEquivalent
			&& Different.Difference
				== "normalized diagnostics differ",
		"differential-result",
		"diagnostic location disagreement was not detected");

	const fs::path CorpusRoot =
		fs::path(ANGELSCRIPT_STANDALONE_CORPUS_ROOT);
	const fs::path RepositoryRoot =
		fs::path(ANGELSCRIPT_STANDALONE_REPOSITORY_ROOT);
	const fs::path FixtureRoot =
		fs::path(ANGELSCRIPT_OFFLINE_FIXTURE_ROOT);
	const std::string Json = ReadText(CorpusRoot / "corpus-index.json");
	rapidjson::Document Index;
	Index.Parse(Json.data(), Json.size());
	if (Index.HasParseError())
	{
		std::cerr
			<< "corpus-index.json: "
			<< rapidjson::GetParseError_En(Index.GetParseError())
			<< '\n';
		return 1;
	}

	bPassed &= Require(
		Index.IsObject()
			&& Index.HasMember("schemaVersion")
			&& Index["schemaVersion"].IsInt()
			&& Index["schemaVersion"].GetInt() == 1
			&& Index.HasMember("cases")
			&& Index["cases"].IsArray(),
		"corpus-index",
		"invalid versioned root");
	if (!bPassed)
	{
		return 1;
	}

	std::set<std::string> Ids;
	std::size_t Automated = 0;
	std::size_t External = 0;
	for (const rapidjson::Value& Entry : Index["cases"].GetArray())
	{
		const bool bShapeValid =
			Entry.IsObject()
			&& Entry.HasMember("id") && Entry["id"].IsString()
			&& Entry.HasMember("source") && Entry["source"].IsString()
			&& Entry.HasMember("sourceRoot")
				&& Entry["sourceRoot"].IsString()
			&& Entry.HasMember("provenance")
				&& Entry["provenance"].IsString()
				&& Entry["provenance"].GetStringLength() != 0
			&& Entry.HasMember("dependencies")
				&& Entry["dependencies"].IsArray()
				&& !Entry["dependencies"].Empty()
			&& Entry.HasMember("profile") && Entry["profile"].IsString()
			&& Entry.HasMember("classification")
				&& Entry["classification"].IsString()
			&& Entry.HasMember("automation")
				&& Entry["automation"].IsString()
			&& Entry.HasMember("expected")
				&& Entry["expected"].IsObject()
			&& Entry.HasMember("evidence")
				&& Entry["evidence"].IsString()
				&& Entry["evidence"].GetStringLength() != 0;
		bPassed &= Require(
			bShapeValid,
			"corpus-entry",
			"missing a required field or evidence");
		if (!bShapeValid)
		{
			continue;
		}

		const std::string Id = Entry["id"].GetString();
		const std::string Classification =
			Entry["classification"].GetString();
		const std::string Automation = Entry["automation"].GetString();
		const std::string Profile = Entry["profile"].GetString();
		const rapidjson::Value& Expected = Entry["expected"];
		bPassed &= Require(
			Ids.emplace(Id).second,
			Id,
			"duplicate corpus ID");
		bPassed &= Require(
			IsKnownClassification(Classification),
			Id,
			"unknown support classification");
		std::set<std::string> Dependencies;
		for (const rapidjson::Value& Dependency
			: Entry["dependencies"].GetArray())
		{
			bPassed &= Require(
				Dependency.IsString()
					&& Dependency.GetStringLength() != 0
					&& Dependencies.emplace(
						Dependency.IsString()
							? Dependency.GetString()
							: "")
						.second,
				Id,
				"dependency must be a non-empty unique string");
		}
		const bool bExpectedShape =
			Expected.HasMember("compile")
			&& Expected["compile"].IsString()
			&& Expected.HasMember("run")
			&& Expected["run"].IsString();
		bPassed &= Require(
			bExpectedShape,
			Id,
			"expected outcome is incomplete");
		if (!bExpectedShape)
		{
			continue;
		}
		if (Classification != "supported")
		{
			bPassed &= Require(
				Entry.HasMember("reason")
					&& Entry["reason"].IsString()
					&& Entry["reason"].GetStringLength() != 0,
				Id,
				"non-supported case has no intentional-difference reason");
		}

		const fs::path Source =
			std::string_view(Entry["sourceRoot"].GetString())
					== "repository"
				? RepositoryRoot / Entry["source"].GetString()
				: CorpusRoot / Entry["source"].GetString();
		bPassed &= Require(
			fs::is_regular_file(Source),
			Id,
			"source path is stale");
		if (!fs::is_regular_file(Source))
		{
			continue;
		}

		if (Automation != "standalone")
		{
			++External;
			const bool bProjectEvidence =
				Automation == "external-project-evidence"
				&& Entry.HasMember("bundle")
				&& Entry["bundle"].IsString()
				&& std::string_view(
					Entry["bundle"].GetString()) == "project";
			bPassed &= Require(
				bProjectEvidence,
				Id,
				"unrecognized or unlabeled automation exclusion");
			if (!bProjectEvidence)
			{
				continue;
			}

			const std::string Evidence = Entry["evidence"].GetString();
			const std::size_t Fragment = Evidence.find('#');
			const std::string EvidencePath =
				Evidence.substr(0, Fragment);
			const std::string EvidenceCase =
				Fragment == std::string::npos
					? std::string()
					: Evidence.substr(Fragment + 1);
			const fs::path EvidenceFile =
				RepositoryRoot / EvidencePath;
			bPassed &= Require(
				fs::is_regular_file(EvidenceFile)
					&& EvidenceCase == Id,
				Id,
				"external evidence file or case fragment is stale");
			if (!fs::is_regular_file(EvidenceFile)
				|| EvidenceCase != Id)
			{
				continue;
			}
			rapidjson::Document EvidenceDocument;
			const std::string EvidenceJson =
				ReadText(EvidenceFile);
			EvidenceDocument.Parse(
				EvidenceJson.data(),
				EvidenceJson.size());
			const bool bEvidenceValid =
				!EvidenceDocument.HasParseError()
				&& EvidenceDocument.IsObject()
				&& EvidenceDocument.HasMember("bundle")
				&& EvidenceDocument["bundle"].IsObject()
				&& EvidenceDocument["bundle"].HasMember(
					"symbolScopeComplete")
				&& EvidenceDocument["bundle"][
					"symbolScopeComplete"].IsBool()
				&& EvidenceDocument["bundle"][
					"symbolScopeComplete"].GetBool()
				&& EvidenceDocument.HasMember("cases")
				&& EvidenceDocument["cases"].IsObject()
				&& EvidenceDocument["cases"].HasMember(
					Id.c_str())
				&& EvidenceDocument["cases"][Id.c_str()].IsObject()
				&& EvidenceDocument["cases"][Id.c_str()].HasMember(
					"status")
				&& EvidenceDocument["cases"][Id.c_str()][
					"status"].IsString()
				&& std::string_view(
					EvidenceDocument["cases"][Id.c_str()][
						"status"].GetString()) == "complete";
			bPassed &= Require(
				bEvidenceValid,
				Id,
				"external evidence is missing a complete result or complete symbol scope");
			continue;
		}
		++Automated;

		const bool bBundleShapeValid =
			Profile == "native"
				? !Entry.HasMember("bundle")
				: Entry.HasMember("bundle")
					&& Entry["bundle"].IsString()
					&& std::string_view(
						Entry["bundle"].GetString())
						== "default-engine";
		bPassed &= Require(
			bBundleShapeValid,
			Id,
			"profile, bundle, and automation requirements disagree");
		const bool bExpectCompile =
			Expected.HasMember("compile")
			&& Expected["compile"].IsString()
			&& std::string_view(Expected["compile"].GetString())
				== "success";
		if (Profile == "native")
		{
			FRunRequest Request;
			Request.ScriptRoots = {Source.parent_path()};
			Request.Entry = Source.filename();
			const FRunResult Result = RunNativeScript(Request);
			const bool bRunSucceeded = Result.ExitCode == 0;
			bPassed &= Require(
				bRunSucceeded == bExpectCompile,
				Id,
				Result.Error.empty()
					? "native outcome differs"
					: Result.Error);
			if (bRunSucceeded)
			{
				bPassed &= Require(
					Expected.HasMember("scriptResult")
						&& Expected["scriptResult"].IsInt()
						&& Result.ScriptResult.has_value()
						&& *Result.ScriptResult
							== Expected["scriptResult"].GetInt(),
					Id,
					"native script result differs");
			}
		}
		else if (Profile == "ue-validation")
		{
			FUECompileRequest Request;
			Request.ScriptRoots = {Source.parent_path()};
			Request.Entry = Source.filename();
			Request.ExplicitBundle = FixtureRoot / "default-engine";
			Request.PackagedDefaultBundle =
				RepositoryRoot / "not-used";
			Request.bEmitByteCode = true;
			const FUECompileResult Result = FUECompiler().Compile(Request);
			bPassed &= Require(
				!Result.bInfrastructureFailure,
				Id,
				Result.Error);
			bPassed &= Require(
				Result.bSuccess == bExpectCompile,
				Id,
				Result.Error.empty()
					? "UE-validation outcome differs"
					: Result.Error);
			if (Result.bSuccess)
			{
				bPassed &= Require(
					!Result.ByteCode.empty(),
					Id,
					"successful UE-validation emitted no bytecode");
			}
		}
		else
		{
			bPassed &= Require(false, Id, "unknown profile");
		}
	}

	bPassed &= Require(
		Automated >= 5 && External >= 1,
		"corpus-index",
		"corpus family coverage or external evidence is missing");
	return bPassed ? 0 : 1;
}
