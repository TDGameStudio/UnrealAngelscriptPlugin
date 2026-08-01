#include "Contract/AngelscriptOfflineBundleLoader.h"
#include "Contract/AngelscriptOfflineManifest.h"
#include "Support/AngelscriptStandaloneHash.h"
#include "Support/AngelscriptStandaloneStreamingHash.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	using namespace AngelscriptStandalone;

	constexpr std::string_view TypeId =
		"1111111111111111111111111111111111111111111111111111111111111111";
	constexpr std::string_view CallableId =
		"2222222222222222222222222222222222222222222222222222222222222222";
	constexpr std::string_view AssetId =
		"3333333333333333333333333333333333333333333333333333333333333333";
	constexpr std::string_view AdapterId =
		"4444444444444444444444444444444444444444444444444444444444444444";

	struct FTestDirectory
	{
		explicit FTestDirectory(const std::string_view Name)
		{
			const auto Suffix = std::chrono::steady_clock::now()
				.time_since_epoch()
				.count();
			Path = std::filesystem::temp_directory_path()
				/ ("angelscript-offline-" + std::string(Name) + "-"
					+ std::to_string(Suffix));
			std::filesystem::create_directories(Path);
		}

		~FTestDirectory()
		{
			std::error_code ErrorCode;
			std::filesystem::remove_all(Path, ErrorCode);
		}

		std::filesystem::path Path;
	};

	bool Require(
		const bool bCondition,
		const std::string_view Test,
		const std::string_view Message)
	{
		if (!bCondition)
		{
			std::cerr << Test << ": " << Message << '\n';
		}
		return bCondition;
	}

	bool Contains(
		const std::string_view Text,
		const std::string_view Needle)
	{
		return Text.find(Needle) != std::string_view::npos;
	}

	std::string MakeTypeRecord(
		const std::string_view Id = TypeId,
		const std::string_view Name = "FWidget",
		const std::string_view OptionalSuffix = {})
	{
		return "{\"canonicalIdentity\":\"type-id\",\"kind\":\"type\","
			"\"origin\":{\"kind\":\"manual\",\"layer\":\"host-surface\","
			"\"module\":\"Engine\",\"plugin\":\"\","
			"\"stableModuleId\":\"engine-module\"},\"schema\":\"1.0\","
			"\"stableId\":\""
			+ std::string(Id)
			+ "\",\"type\":{\"adapterStableId\":\""
			+ std::string(AdapterId)
			+ "\",\"baseStableId\":\"\",\"completeDeclaration\":\"class "
			+ std::string(Name)
			+ "\",\"interfaces\":[],\"kind\":\"class\",\"members\":[\""
			+ std::string(CallableId)
			+ "\"],\"name\":\""
			+ std::string(Name)
			+ "\",\"namespace\":\"UE\",\"stableId\":\""
			+ std::string(Id)
			+ "\",\"ueTypePath\":\"/Script/Engine."
			+ std::string(Name) + "\"}"
			+ std::string(OptionalSuffix) + "}";
	}

	std::string MakeCallableRecord(
		const std::string_view Id = CallableId,
		const std::string_view OwnerId = TypeId)
	{
		return "{\"callable\":{\"adapterStableId\":\"\","
			"\"declaration\":\"void Tick(float DeltaSeconds)\","
			"\"kind\":\"method\",\"name\":\"Tick\",\"namespace\":\"UE\","
			"\"ownerStableId\":\""
			+ std::string(OwnerId)
			+ "\",\"returnType\":\"void\",\"stableId\":\""
			+ std::string(Id)
			+ "\",\"ueFunctionPath\":\"/Script/Engine.FWidget:Tick\"},"
			"\"canonicalIdentity\":\"callable-id\",\"kind\":\"callable\","
			"\"origin\":{\"kind\":\"generated\",\"layer\":\"host-surface\","
			"\"module\":\"Engine\",\"plugin\":\"\","
			"\"stableModuleId\":\"engine-module\"},\"schema\":\"1.0\","
			"\"stableId\":\""
			+ std::string(Id) + "\"}";
	}

	std::string MakeAssetRecord(
		const std::string_view Id = AssetId)
	{
		return "{\"assetClassPath\":\"/Script/Engine.Blueprint\","
			"\"availability\":\"available\","
			"\"baseClassPath\":\"/Script/Engine.Actor\","
			"\"generatedClassPath\":\"/Game/BP_Widget.BP_Widget_C\","
			"\"mountPoint\":\"/Game\","
			"\"objectPath\":\"/Game/BP_Widget.BP_Widget\","
			"\"originModule\":\"Project\",\"originPlugin\":\"\","
			"\"packagePath\":\"/Game\","
			"\"redirectSource\":\"\",\"redirectTarget\":\"\","
			"\"schema\":\"1.0\",\"stableId\":\""
			+ std::string(Id)
			+ "\",\"typeCheckTags\":{\"baseClass\":\"/Script/Engine.Actor\"}}";
	}

	struct FBundleFiles
	{
		std::string Symbols;
		std::string Assets;
		std::string Manifest;
	};

	std::string MakeManifest(
		const std::string_view Symbols,
		const std::string_view Assets,
		const std::string_view Kind = "project",
		const int Major = 1,
		const int Minor = 0,
		const bool bCompleteSymbols = true,
		const std::string_view RequiredFields =
			"\"manifest.schema\",\"manifest.symbolScope\",\"records.stableId\"")
	{
		const std::string SymbolHash = Sha256(Symbols);
		const std::string AssetHash = Sha256(Assets);
		const std::string Identity = Sha256(
			"offline-bundle-v1\n" + std::string(Kind) + "\n"
			+ std::to_string(Major) + "." + std::to_string(Minor) + "\n"
			+ SymbolHash + "\n" + AssetHash);
		const auto CountRecords = [](const std::string_view Text)
		{
			std::uint64_t Count = 0;
			for (const char Character : Text)
			{
				Count += Character == '\n';
			}
			return Count;
		};
		return "{\"adapters\":[{\"declarativeOnly\":true,"
			"\"name\":\"TArray\","
			"\"requiredEngineProperties\":[\"angelscript.fork\"],"
			"\"requiredTraits\":[\"element.copy\"],\"stableId\":\""
			+ std::string(AdapterId)
			+ "\",\"surfaceHash\":\"5555555555555555555555555555555555555555555555555555555555555555\","
			"\"version\":\"1\"}],"
			"\"assetScope\":{\"complete\":true,\"included\":[\"/Game\"],"
			"\"skipped\":[],\"state\":\"asset-registry-complete\"},"
			"\"bundleIdentity\":\""
			+ Identity + "\",\"bundleKind\":\"" + std::string(Kind)
			+ "\",\"compilerContractVersion\":\"ue-as-standalone-v1\","
			"\"configuration\":\"Development\","
			"\"engineProperties\":{\"angelscript.fork\":\"2.33+selective-2.38\","
			"\"unreal.major\":\"5\",\"unreal.minor\":\"8\"},"
			"\"featureFlags\":{\"editor\":true},\"files\":["
			"{\"byteCount\":" + std::to_string(Assets.size())
			+ ",\"name\":\"assets.jsonl\",\"recordCount\":"
			+ std::to_string(CountRecords(Assets))
			+ ",\"sha256\":\"" + AssetHash + "\"},"
			"{\"byteCount\":" + std::to_string(Symbols.size())
			+ ",\"name\":\"symbols.jsonl\",\"recordCount\":"
			+ std::to_string(CountRecords(Symbols))
			+ ",\"sha256\":\"" + SymbolHash + "\"}],"
			"\"forkVersion\":\"2.33+selective-2.38\","
			"\"loadedModules\":[\"Engine\"],"
			"\"loadedPlugins\":[\"Angelscript\"],"
			"\"platform\":\"WindowsEditor\",\"pluginVersion\":\"1.0.0\","
			"\"producerName\":\"AngelscriptRuntime\","
			"\"producerVersion\":\"1.0.0\",\"requiredFields\":["
			+ std::string(RequiredFields)
			+ "],\"schema\":{\"major\":" + std::to_string(Major)
			+ ",\"minor\":" + std::to_string(Minor)
			+ "},\"symbolScope\":{\"complete\":"
			+ (bCompleteSymbols ? std::string("true") : std::string("false"))
			+ ",\"included\":[\"asIScriptEngine.final-registration\"],"
			"\"skipped\":[],\"state\":\"host-surface\"},"
			"\"unrealVersion\":\"5.8.0\"}";
	}

	FBundleFiles MakeValidBundleFiles(
		const bool bReverseSymbols = false,
		const std::string_view Kind = "project")
	{
		FBundleFiles Files;
		Files.Symbols = bReverseSymbols
			? MakeCallableRecord() + "\n" + MakeTypeRecord() + "\n"
			: MakeTypeRecord() + "\n" + MakeCallableRecord() + "\n";
		Files.Assets = MakeAssetRecord() + "\n";
		Files.Manifest = MakeManifest(Files.Symbols, Files.Assets, Kind);
		return Files;
	}

	void WriteFile(
		const std::filesystem::path& Path,
		const std::string_view Bytes)
	{
		std::ofstream Output(Path, std::ios::binary | std::ios::trunc);
		Output.write(
			Bytes.data(),
			static_cast<std::streamsize>(Bytes.size()));
	}

	void WriteBundle(
		const std::filesystem::path& Directory,
		const FBundleFiles& Files)
	{
		std::filesystem::create_directories(Directory);
		WriteFile(Directory / "symbols.jsonl", Files.Symbols);
		WriteFile(Directory / "assets.jsonl", Files.Assets);
		WriteFile(Directory / "manifest.json", Files.Manifest);
	}

	bool TestStreamingSha256()
	{
		constexpr std::string_view Name = "StreamingSha256";
		FStreamingSha256 Empty;
		bool bPassed = Require(
			Empty.Finish() == Sha256(""),
			Name,
			"empty input differs from one-shot SHA-256");

		FStreamingSha256 Split;
		Split.Update("a");
		const std::vector<std::uint8_t> Remainder = {'b', 'c'};
		Split.Update(std::span<const std::uint8_t>(Remainder));
		bPassed &= Require(
			Split.Finish() == Sha256("abc"),
			Name,
			"chunked input differs from one-shot SHA-256");
		return bPassed;
	}

	bool TestManifestCompatibility()
	{
		constexpr std::string_view Name = "ManifestCompatibility";
		const FBundleFiles Files = MakeValidBundleFiles();
		bool bPassed = Require(
			ParseOfflineManifest(Files.Manifest).bSuccess,
			Name,
			"valid manifest was rejected");

		FOfflineManifestCompatibility Compatibility;
		Compatibility.ExpectedForkVersion = "different";
		auto Result = ParseOfflineManifest(Files.Manifest, Compatibility);
		bPassed &= Require(
			!Result.bSuccess && Contains(Result.Error, "fork"),
			Name,
			"fork incompatibility was not rejected");

		Result = ParseOfflineManifest(
			MakeManifest(Files.Symbols, Files.Assets, "project", 2, 0));
		bPassed &= Require(
			!Result.bSuccess && Contains(Result.Error, "schema major"),
			Name,
			"schema major incompatibility was not rejected");

		Result = ParseOfflineManifest(
			MakeManifest(Files.Symbols, Files.Assets, "project", 1, 1));
		bPassed &= Require(
			!Result.bSuccess && Contains(Result.Error, "schema minor"),
			Name,
			"schema minor incompatibility was not rejected");

		Result = ParseOfflineManifest(
			MakeManifest(
				Files.Symbols,
				Files.Assets,
				"project",
				1,
				0,
				false));
		bPassed &= Require(
			!Result.bSuccess && Contains(Result.Error, "incomplete"),
			Name,
			"incomplete symbol scope was not rejected");

		Result = ParseOfflineManifest(
			MakeManifest(
				Files.Symbols,
				Files.Assets,
				"project",
				1,
				0,
				true,
				"\"future.required\""));
		bPassed &= Require(
			!Result.bSuccess && Contains(Result.Error, "required field"),
			Name,
			"unsupported required field was not rejected");
		return bPassed;
	}

	bool TestValidBundleAndIndices()
	{
		constexpr std::string_view Name = "ValidBundleAndIndices";
		FTestDirectory Directory(Name);
		WriteBundle(Directory.Path, MakeValidBundleFiles(true));
		const FOfflineBundleLoadResult Result =
			LoadOfflineBundle(Directory.Path);
		bool bPassed = Require(
			Result.bSuccess,
			Name,
			Result.Error);
		if (!Result.bSuccess)
		{
			return false;
		}
		bPassed &= Require(
			Result.Bundle.Indices->Symbols().size() == 2,
			Name,
			"wrong symbol count");
		bPassed &= Require(
			Result.Bundle.Indices->Symbols()[0].StableId == TypeId
				&& Result.Bundle.Indices->Symbols()[1].StableId == CallableId,
			Name,
			"indices did not normalize symbol order by stable ID");
		bPassed &= Require(
			Result.Bundle.Indices->FindSymbol(TypeId) != nullptr,
			Name,
			"stable ID lookup failed");
		bPassed &= Require(
			Result.Bundle.Indices->FindType(TypeId) != nullptr
				&& Result.Bundle.Indices->FindType(CallableId) == nullptr,
			Name,
			"type lookup did not preserve symbol kind");
		bPassed &= Require(
			Result.Bundle.Indices->FindCallable(CallableId) != nullptr
				&& Result.Bundle.Indices->FindCallable(TypeId) == nullptr,
			Name,
			"callable lookup did not preserve symbol kind");
		bPassed &= Require(
			Result.Bundle.Indices->FindOwnedSymbols(TypeId).size() == 1,
			Name,
			"owner lookup failed");
		bPassed &= Require(
			Result.Bundle.Indices->FindNamespaceSymbols("UE").size() == 2,
			Name,
			"namespace lookup failed");
		bPassed &= Require(
			Result.Bundle.Indices->FindModuleSymbols("engine-module").size() == 2,
			Name,
			"module lookup failed");
		bPassed &= Require(
			Result.Bundle.Indices->FindAdapter(AdapterId) != nullptr,
			Name,
			"adapter lookup failed");
		bPassed &= Require(
			Result.Bundle.Indices->FindAssetByObjectPath(
				"/Game/BP_Widget.BP_Widget") != nullptr,
			Name,
			"asset object path lookup failed");
		bPassed &= Require(
			Result.Bundle.Indices->FindAssetByGeneratedClassPath(
				"/Game/BP_Widget.BP_Widget_C") != nullptr,
			Name,
			"generated class lookup failed");
		return bPassed;
	}

	bool TestFrozenProducerFixtures()
	{
		constexpr std::string_view Name = "FrozenProducerFixtures";
		const std::filesystem::path Root =
			ANGELSCRIPT_OFFLINE_FIXTURE_ROOT;
		const FOfflineBundleLoadResult Default =
			LoadOfflineBundle(Root / "default-engine");
		const FOfflineBundleLoadResult Project =
			LoadOfflineBundle(Root / "project");
		bool bPassed = Require(
			Default.bSuccess,
			Name,
			Default.Error);
		bPassed &= Require(
			Project.bSuccess,
			Name,
			Project.Error);
		if (Default.bSuccess && Project.bSuccess)
		{
			bPassed &= Require(
				Default.Bundle.Manifest.BundleKind
					== EOfflineBundleKind::DefaultEngine
					&& Project.Bundle.Manifest.BundleKind
						== EOfflineBundleKind::Project,
				Name,
				"producer fixture bundle kinds were not preserved");
			bPassed &= Require(
				Default.Bundle.Indices->Symbols().size() == 1
					&& Project.Bundle.Indices->Symbols().size() == 1
					&& Default.Bundle.Indices->Assets().empty()
					&& Project.Bundle.Indices->Assets().empty(),
				Name,
				"producer fixture record counts disagree with manifests");
			bPassed &= Require(
				Default.Bundle.Manifest.BundleIdentity
					!= Project.Bundle.Manifest.BundleIdentity,
				Name,
				"producer fixture kind did not affect bundle identity");
		}
		return bPassed;
	}

	bool TestUnknownOptionalFields()
	{
		constexpr std::string_view Name = "UnknownOptionalFields";
		FTestDirectory Directory(Name);
		FBundleFiles Files = MakeValidBundleFiles();
		Files.Symbols =
			MakeTypeRecord(TypeId, "FWidget", ",\"futureOptional\":{\"x\":1}")
			+ "\n" + MakeCallableRecord() + "\n";
		Files.Manifest = MakeManifest(Files.Symbols, Files.Assets);
		const std::string BeforeLastBrace =
			Files.Manifest.substr(0, Files.Manifest.size() - 1);
		Files.Manifest =
			BeforeLastBrace + ",\"futureOptional\":{\"enabled\":true}}";
		WriteBundle(Directory.Path, Files);
		const FOfflineBundleLoadResult Result =
			LoadOfflineBundle(Directory.Path);
		return Require(
			Result.bSuccess,
			Name,
			Result.Error);
	}

	bool TestSelectionNeverFallsBack()
	{
		constexpr std::string_view Name = "SelectionNeverFallsBack";
		FTestDirectory Directory(Name);
		const std::filesystem::path Packaged = Directory.Path / "packaged";
		const std::filesystem::path Missing = Directory.Path / "missing";
		WriteBundle(Packaged, MakeValidBundleFiles(false, "default-engine"));

		const FOfflineBundleLoadResult DefaultResult =
			LoadSelectedOfflineBundle(std::nullopt, Packaged);
		bool bPassed = Require(
			DefaultResult.bSuccess
				&& DefaultResult.Bundle.Manifest.BundleKind
					== EOfflineBundleKind::DefaultEngine,
			Name,
			DefaultResult.Error);

		const FOfflineBundleLoadResult ExplicitResult =
			LoadSelectedOfflineBundle(Missing, Packaged);
		bPassed &= Require(
			!ExplicitResult.bSuccess
				&& Contains(ExplicitResult.Error, "explicit"),
			Name,
			"invalid explicit bundle silently fell back to packaged default");
		return bPassed;
	}

	bool TestFileShapeFailures()
	{
		constexpr std::string_view Name = "FileShapeFailures";
		bool bPassed = true;
		{
			FTestDirectory Directory("missing");
			FBundleFiles Files = MakeValidBundleFiles();
			WriteBundle(Directory.Path, Files);
			std::filesystem::remove(Directory.Path / "assets.jsonl");
			const auto Result = LoadOfflineBundle(Directory.Path);
			bPassed &= Require(
				!Result.bSuccess && Contains(Result.Error, "missing"),
				Name,
				"missing required file was accepted");
		}
		{
			FTestDirectory Directory("extra");
			WriteBundle(Directory.Path, MakeValidBundleFiles());
			WriteFile(Directory.Path / "extra.txt", "unexpected");
			const auto Result = LoadOfflineBundle(Directory.Path);
			bPassed &= Require(
				!Result.bSuccess && Contains(Result.Error, "unexpected"),
				Name,
				"unexpected bundle file was accepted");
		}
		{
			FTestDirectory Directory("no-lf");
			FBundleFiles Files = MakeValidBundleFiles();
			Files.Symbols.pop_back();
			Files.Manifest = MakeManifest(Files.Symbols, Files.Assets);
			WriteBundle(Directory.Path, Files);
			const auto Result = LoadOfflineBundle(Directory.Path);
			bPassed &= Require(
				!Result.bSuccess && Contains(Result.Error, "end with an LF"),
				Name,
				"unterminated JSONL record was accepted");
		}
		{
			FTestDirectory Directory("crlf");
			FBundleFiles Files = MakeValidBundleFiles();
			const std::size_t Newline = Files.Symbols.find('\n');
			Files.Symbols.insert(Newline, 1, '\r');
			Files.Manifest = MakeManifest(Files.Symbols, Files.Assets);
			WriteBundle(Directory.Path, Files);
			const auto Result = LoadOfflineBundle(Directory.Path);
			bPassed &= Require(
				!Result.bSuccess && Contains(Result.Error, "LF line endings"),
				Name,
				"CRLF JSONL was accepted");
		}
		return bPassed;
	}

	bool TestIntegrityFailures()
	{
		constexpr std::string_view Name = "IntegrityFailures";
		bool bPassed = true;
		{
			FTestDirectory Directory("hash");
			FBundleFiles Files = MakeValidBundleFiles();
			WriteBundle(Directory.Path, Files);
			const std::size_t Widget = Files.Symbols.find("FWidget");
			Files.Symbols.replace(Widget, 7, "GWidget");
			WriteFile(Directory.Path / "symbols.jsonl", Files.Symbols);
			const auto Result = LoadOfflineBundle(Directory.Path);
			bPassed &= Require(
				!Result.bSuccess && Contains(Result.Error, "SHA-256 mismatch"),
				Name,
				"content hash mismatch was accepted");
		}
		{
			FTestDirectory Directory("identity");
			FBundleFiles Files = MakeValidBundleFiles();
			const std::size_t Identity = Files.Manifest.find(
				"\"bundleIdentity\":\"");
			const std::size_t Value = Identity
				+ std::string_view("\"bundleIdentity\":\"").size();
			Files.Manifest[Value] =
				Files.Manifest[Value] == '0' ? '1' : '0';
			WriteBundle(Directory.Path, Files);
			const auto Result = LoadOfflineBundle(Directory.Path);
			bPassed &= Require(
				!Result.bSuccess && Contains(Result.Error, "identity mismatch"),
				Name,
				"bundle identity mismatch was accepted");
		}
		return bPassed;
	}

	bool TestMalformedAndInvalidUtf8()
	{
		constexpr std::string_view Name = "MalformedAndInvalidUtf8";
		bool bPassed = true;
		{
			FTestDirectory Directory("malformed");
			FBundleFiles Files = MakeValidBundleFiles();
			Files.Symbols = "{\"schema\":\"1.0\"\n";
			Files.Manifest = MakeManifest(Files.Symbols, Files.Assets);
			WriteBundle(Directory.Path, Files);
			const auto Result = LoadOfflineBundle(Directory.Path);
			bPassed &= Require(
				!Result.bSuccess && Contains(Result.Error, "invalid JSONL"),
				Name,
				"malformed JSONL was accepted");
		}
		{
			FTestDirectory Directory("utf8");
			FBundleFiles Files = MakeValidBundleFiles();
			const std::size_t Widget = Files.Symbols.find("FWidget");
			Files.Symbols[Widget] = static_cast<char>(0xff);
			Files.Manifest = MakeManifest(Files.Symbols, Files.Assets);
			WriteBundle(Directory.Path, Files);
			const auto Result = LoadOfflineBundle(Directory.Path);
			bPassed &= Require(
				!Result.bSuccess && Contains(Result.Error, "invalid JSONL"),
				Name,
				"invalid UTF-8 JSONL was accepted");
		}
		return bPassed;
	}

	bool TestDuplicateStableIds()
	{
		constexpr std::string_view Name = "DuplicateStableIds";
		bool bPassed = true;
		{
			FTestDirectory Directory("duplicate");
			FBundleFiles Files = MakeValidBundleFiles();
			Files.Symbols =
				MakeTypeRecord() + "\n" + MakeTypeRecord() + "\n";
			Files.Manifest = MakeManifest(Files.Symbols, Files.Assets);
			WriteBundle(Directory.Path, Files);
			const auto Result = LoadOfflineBundle(Directory.Path);
			bPassed &= Require(
				!Result.bSuccess && Contains(Result.Error, "duplicate stable ID"),
				Name,
				"identical duplicate stable ID was accepted");
		}
		{
			FTestDirectory Directory("inconsistent");
			FBundleFiles Files = MakeValidBundleFiles();
			Files.Symbols =
				MakeTypeRecord() + "\n"
				+ MakeTypeRecord(TypeId, "FOther") + "\n";
			Files.Manifest = MakeManifest(Files.Symbols, Files.Assets);
			WriteBundle(Directory.Path, Files);
			const auto Result = LoadOfflineBundle(Directory.Path);
			bPassed &= Require(
				!Result.bSuccess
					&& Contains(Result.Error, "inconsistent duplicate"),
				Name,
				"inconsistent duplicate stable ID was accepted");
		}
		return bPassed;
	}
}

int main(const int ArgumentCount, char** Arguments)
{
	bool bPassed = true;
	bPassed &= TestStreamingSha256();
	bPassed &= TestManifestCompatibility();
	bPassed &= TestValidBundleAndIndices();
	bPassed &= TestFrozenProducerFixtures();
	bPassed &= TestUnknownOptionalFields();
	bPassed &= TestSelectionNeverFallsBack();
	bPassed &= TestFileShapeFailures();
	bPassed &= TestIntegrityFailures();
	bPassed &= TestMalformedAndInvalidUtf8();
	bPassed &= TestDuplicateStableIds();
	if (ArgumentCount == 2)
	{
		const FOfflineBundleLoadResult Result =
			LoadOfflineBundle(std::filesystem::path(Arguments[1]));
		bPassed &= Require(
			Result.bSuccess,
			"ExternalProducerBundle",
			Result.Error);
		if (Result.bSuccess)
		{
			std::cout
				<< "external bundle: "
				<< Result.Bundle.Indices->Symbols().size()
				<< " symbols, "
				<< Result.Bundle.Indices->Assets().size()
				<< " assets\n";
		}
	}
	return bPassed ? 0 : 1;
}
