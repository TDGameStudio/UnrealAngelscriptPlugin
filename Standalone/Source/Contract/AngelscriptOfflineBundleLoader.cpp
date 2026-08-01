#include "Contract/AngelscriptOfflineBundleLoader.h"

#include "Support/AngelscriptStandaloneHash.h"
#include "Support/AngelscriptStandaloneStreamingHash.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <set>
#include <span>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace AngelscriptStandalone
{
	namespace
	{
		constexpr std::array<std::string_view, 3> RequiredFileNames = {
			"assets.jsonl",
			"manifest.json",
			"symbols.jsonl",
		};

		bool IsLowerHexSha256(const std::string_view Value)
		{
			if (Value.size() != 64)
			{
				return false;
			}
			for (const char Character : Value)
			{
				if (!((Character >= '0' && Character <= '9')
					|| (Character >= 'a' && Character <= 'f')))
				{
					return false;
				}
			}
			return true;
		}

		std::string PathForError(const std::filesystem::path& Path)
		{
			return Path.generic_string();
		}

		bool ValidateBundleDirectory(
			const std::filesystem::path& Directory,
			const bool bRejectUnexpectedEntries,
			std::string& OutError)
		{
			std::error_code ErrorCode;
			const std::filesystem::file_status DirectoryStatus =
				std::filesystem::symlink_status(Directory, ErrorCode);
			if (ErrorCode || !std::filesystem::is_directory(DirectoryStatus))
			{
				OutError = "offline bundle directory does not exist: "
					+ PathForError(Directory);
				return false;
			}
			if (std::filesystem::is_symlink(DirectoryStatus))
			{
				OutError = "offline bundle directory must not be a symbolic link: "
					+ PathForError(Directory);
				return false;
			}

			std::set<std::string> Entries;
			for (std::filesystem::directory_iterator Iterator(
					Directory,
					ErrorCode);
				!ErrorCode && Iterator != std::filesystem::directory_iterator();
				Iterator.increment(ErrorCode))
			{
				const std::filesystem::directory_entry& Entry = *Iterator;
				const std::string Name = Entry.path().filename().generic_string();
				const std::filesystem::file_status Status =
					Entry.symlink_status(ErrorCode);
				if (ErrorCode)
				{
					break;
				}
				if (std::filesystem::is_symlink(Status)
					|| !std::filesystem::is_regular_file(Status))
				{
					OutError = "offline bundle contains a non-regular entry: "
						+ Name;
					return false;
				}
				Entries.insert(Name);
			}
			if (ErrorCode)
			{
				OutError = "failed to enumerate offline bundle directory: "
					+ ErrorCode.message();
				return false;
			}

			for (const std::string_view Required : RequiredFileNames)
			{
				if (!Entries.contains(std::string(Required)))
				{
					OutError = "offline bundle is missing required file: "
						+ std::string(Required);
					return false;
				}
			}
			if (bRejectUnexpectedEntries
				&& Entries.size() != RequiredFileNames.size())
			{
				for (const std::string& Entry : Entries)
				{
					bool bExpected = false;
					for (const std::string_view Required : RequiredFileNames)
					{
						bExpected |= Entry == Required;
					}
					if (!bExpected)
					{
						OutError =
							"offline bundle contains an unexpected entry: "
							+ Entry;
						return false;
					}
				}
			}
			return true;
		}

		bool ReadSmallFile(
			const std::filesystem::path& Path,
			const std::uint64_t MaximumBytes,
			std::string& OutBytes,
			std::string& OutError)
		{
			std::error_code ErrorCode;
			const std::uintmax_t FileSize =
				std::filesystem::file_size(Path, ErrorCode);
			if (ErrorCode)
			{
				OutError = "failed to inspect " + PathForError(Path)
					+ ": " + ErrorCode.message();
				return false;
			}
			if (FileSize > MaximumBytes
				|| FileSize
					> static_cast<std::uintmax_t>(
						std::numeric_limits<std::size_t>::max()))
			{
				OutError = "offline manifest exceeds the configured size limit";
				return false;
			}

			std::ifstream Input(Path, std::ios::binary);
			if (!Input)
			{
				OutError = "failed to open " + PathForError(Path);
				return false;
			}
			OutBytes.resize(static_cast<std::size_t>(FileSize));
			if (!OutBytes.empty())
			{
				Input.read(
					OutBytes.data(),
					static_cast<std::streamsize>(OutBytes.size()));
			}
			if (!Input || Input.gcount()
				!= static_cast<std::streamsize>(OutBytes.size()))
			{
				OutError = "failed to read complete file: "
					+ PathForError(Path);
				return false;
			}
			return true;
		}

		const FOfflineFileDescriptor* FindDescriptor(
			const FOfflineManifest& Manifest,
			const std::string_view Name)
		{
			for (const FOfflineFileDescriptor& File : Manifest.Files)
			{
				if (File.Name == Name)
				{
					return &File;
				}
			}
			return nullptr;
		}

		template <typename Record, typename ParseRecord>
		bool ReadJsonLines(
			const std::filesystem::path& Path,
			const FOfflineFileDescriptor& Descriptor,
			const std::uint64_t MaximumRecordBytes,
			ParseRecord&& Parser,
			std::vector<Record>& OutRecords,
			std::string& OutError)
		{
			std::ifstream Input(Path, std::ios::binary);
			if (!Input)
			{
				OutError = "failed to open " + PathForError(Path);
				return false;
			}

			FStreamingSha256 Hash;
			std::array<char, 64u * 1024u> Buffer = {};
			std::string Line;
			std::uint64_t ByteCount = 0;
			std::uint64_t RecordCount = 0;
			std::unordered_map<std::string, std::string> RawByStableId;
			bool bFirstBytes = true;

			while (Input)
			{
				Input.read(
					Buffer.data(),
					static_cast<std::streamsize>(Buffer.size()));
				const std::streamsize ReadCount = Input.gcount();
				if (ReadCount <= 0)
				{
					break;
				}
				const auto Bytes = std::span<const std::uint8_t>(
					reinterpret_cast<const std::uint8_t*>(Buffer.data()),
					static_cast<std::size_t>(ReadCount));
				Hash.Update(Bytes);
				ByteCount += static_cast<std::uint64_t>(ReadCount);

				if (bFirstBytes)
				{
					bFirstBytes = false;
					if (ReadCount >= 3
						&& static_cast<unsigned char>(Buffer[0]) == 0xefu
						&& static_cast<unsigned char>(Buffer[1]) == 0xbbu
						&& static_cast<unsigned char>(Buffer[2]) == 0xbfu)
					{
						OutError = Descriptor.Name
							+ " must be UTF-8 without BOM";
						return false;
					}
				}

				for (std::streamsize Index = 0;
					Index < ReadCount;
					++Index)
				{
					const char Character =
						Buffer[static_cast<std::size_t>(Index)];
					if (Character == '\r')
					{
						OutError = Descriptor.Name
							+ " must use LF line endings";
						return false;
					}
					if (Character != '\n')
					{
						Line.push_back(Character);
						if (Line.size() > MaximumRecordBytes)
						{
							OutError = Descriptor.Name
								+ " contains a record exceeding the configured size limit";
							return false;
						}
						continue;
					}

					++RecordCount;
					if (Line.empty())
					{
						OutError = Descriptor.Name
							+ " contains an empty record at line "
							+ std::to_string(RecordCount);
						return false;
					}
					auto Parsed = Parser(std::string_view(Line));
					if (!Parsed.bSuccess)
					{
						OutError = Descriptor.Name + " line "
							+ std::to_string(RecordCount) + ": "
							+ Parsed.Error;
						return false;
					}
					const auto Duplicate = RawByStableId.find(
						Parsed.Record.StableId);
					if (Duplicate != RawByStableId.end())
					{
						OutError = Descriptor.Name
							+ (Duplicate->second == Line
								? " contains a duplicate stable ID at line "
								: " contains an inconsistent duplicate stable ID at line ")
							+ std::to_string(RecordCount)
							+ ": " + Parsed.Record.StableId;
						return false;
					}
					RawByStableId.emplace(
						Parsed.Record.StableId,
						Line);
					OutRecords.emplace_back(std::move(Parsed.Record));
					Line.clear();
				}
			}
			if (Input.bad())
			{
				OutError = "failed while reading " + PathForError(Path);
				return false;
			}
			if (!Line.empty())
			{
				OutError = Descriptor.Name
					+ " must end with an LF record terminator";
				return false;
			}
			if (ByteCount != Descriptor.ByteCount)
			{
				OutError = Descriptor.Name + " byte count mismatch";
				return false;
			}
			if (RecordCount != Descriptor.RecordCount)
			{
				OutError = Descriptor.Name + " record count mismatch";
				return false;
			}
			const std::string ActualHash = Hash.Finish();
			if (ActualHash != Descriptor.Sha256)
			{
				OutError = Descriptor.Name + " SHA-256 mismatch";
				return false;
			}
			return true;
		}

		std::string BundleKindName(const EOfflineBundleKind Kind)
		{
			return Kind == EOfflineBundleKind::Project
				? "project"
				: "default-engine";
		}

		bool ValidateBundleIdentity(
			const FOfflineManifest& Manifest,
			std::string& OutError)
		{
			const FOfflineFileDescriptor* Symbols =
				FindDescriptor(Manifest, "symbols.jsonl");
			const FOfflineFileDescriptor* Assets =
				FindDescriptor(Manifest, "assets.jsonl");
			if (Symbols == nullptr || Assets == nullptr)
			{
				OutError = "manifest is missing a bundle identity input";
				return false;
			}
			if (!IsLowerHexSha256(Manifest.BundleIdentity)
				|| !IsLowerHexSha256(Symbols->Sha256)
				|| !IsLowerHexSha256(Assets->Sha256))
			{
				OutError =
					"bundle identity and file hashes must be lowercase SHA-256";
				return false;
			}
			const std::string IdentityInput =
				"offline-bundle-v1\n"
				+ BundleKindName(Manifest.BundleKind) + "\n"
				+ std::to_string(Manifest.SchemaMajor) + "."
				+ std::to_string(Manifest.SchemaMinor) + "\n"
				+ Symbols->Sha256 + "\n"
				+ Assets->Sha256;
			if (Sha256(IdentityInput) != Manifest.BundleIdentity)
			{
				OutError = "offline bundle identity mismatch";
				return false;
			}
			return true;
		}
	}

	FOfflineBundleSelectionResult SelectOfflineBundleDirectory(
		const std::optional<std::filesystem::path>& ExplicitDirectory,
		const std::filesystem::path& PackagedDefaultDirectory)
	{
		FOfflineBundleSelectionResult Result;
		Result.Source = ExplicitDirectory.has_value()
			? EOfflineBundleSelectionSource::Explicit
			: EOfflineBundleSelectionSource::PackagedDefault;
		Result.Directory = ExplicitDirectory.has_value()
			? *ExplicitDirectory
			: PackagedDefaultDirectory;
		if (Result.Directory.empty())
		{
			Result.Error = ExplicitDirectory.has_value()
				? "explicit offline bundle directory is empty"
				: "packaged default offline bundle directory is empty";
			return Result;
		}
		std::error_code ErrorCode;
		if (!std::filesystem::is_directory(Result.Directory, ErrorCode)
			|| ErrorCode)
		{
			Result.Error = ExplicitDirectory.has_value()
				? "explicit offline bundle directory is unavailable: "
				: "packaged default offline bundle directory is unavailable: ";
			Result.Error += PathForError(Result.Directory);
			return Result;
		}
		Result.bSuccess = true;
		return Result;
	}

	FOfflineBundleLoadResult LoadOfflineBundle(
		const std::filesystem::path& Directory,
		const FOfflineBundleLoadOptions& Options)
	{
		FOfflineBundleLoadResult Result;
		if (!ValidateBundleDirectory(
				Directory,
				Options.bRejectUnexpectedEntries,
				Result.Error))
		{
			return Result;
		}

		std::string ManifestBytes;
		if (!ReadSmallFile(
				Directory / "manifest.json",
				Options.MaximumManifestBytes,
				ManifestBytes,
				Result.Error))
		{
			return Result;
		}
		FOfflineManifestParseResult ParsedManifest =
			ParseOfflineManifest(ManifestBytes, Options.Compatibility);
		if (!ParsedManifest.bSuccess)
		{
			Result.Error = std::move(ParsedManifest.Error);
			return Result;
		}
		if (!ValidateBundleIdentity(ParsedManifest.Manifest, Result.Error))
		{
			return Result;
		}

		std::vector<FOfflineSymbolRecord> Symbols;
		std::vector<FOfflineAssetRecord> Assets;
		const FOfflineFileDescriptor* SymbolDescriptor =
			FindDescriptor(ParsedManifest.Manifest, "symbols.jsonl");
		const FOfflineFileDescriptor* AssetDescriptor =
			FindDescriptor(ParsedManifest.Manifest, "assets.jsonl");
		if (SymbolDescriptor == nullptr || AssetDescriptor == nullptr)
		{
			Result.Error =
				"manifest must describe symbols.jsonl and assets.jsonl";
			return Result;
		}
		if (SymbolDescriptor->RecordCount
			> static_cast<std::uint64_t>(
				std::numeric_limits<std::size_t>::max())
			|| AssetDescriptor->RecordCount
				> static_cast<std::uint64_t>(
					std::numeric_limits<std::size_t>::max()))
		{
			Result.Error = "offline bundle record count exceeds host limits";
			return Result;
		}
		Symbols.reserve(
			static_cast<std::size_t>(SymbolDescriptor->RecordCount));
		Assets.reserve(
			static_cast<std::size_t>(AssetDescriptor->RecordCount));

		if (!ReadJsonLines<FOfflineSymbolRecord>(
				Directory / "symbols.jsonl",
				*SymbolDescriptor,
				Options.MaximumRecordBytes,
				ParseOfflineSymbolRecord,
				Symbols,
				Result.Error)
			|| !ReadJsonLines<FOfflineAssetRecord>(
				Directory / "assets.jsonl",
				*AssetDescriptor,
				Options.MaximumRecordBytes,
				ParseOfflineAssetRecord,
				Assets,
				Result.Error))
		{
			return Result;
		}

		std::shared_ptr<const FOfflineBundleIndices> Indices =
			FOfflineBundleIndices::Build(
				std::move(Symbols),
				std::move(Assets),
				ParsedManifest.Manifest.Adapters,
				Result.Error);
		if (!Indices)
		{
			return Result;
		}

		Result.Bundle.Directory = Directory;
		Result.Bundle.Manifest = std::move(ParsedManifest.Manifest);
		Result.Bundle.Indices = std::move(Indices);
		Result.bSuccess = true;
		return Result;
	}

	FOfflineBundleLoadResult LoadSelectedOfflineBundle(
		const std::optional<std::filesystem::path>& ExplicitDirectory,
		const std::filesystem::path& PackagedDefaultDirectory,
		const FOfflineBundleLoadOptions& Options)
	{
		const FOfflineBundleSelectionResult Selection =
			SelectOfflineBundleDirectory(
				ExplicitDirectory,
				PackagedDefaultDirectory);
		if (!Selection.bSuccess)
		{
			FOfflineBundleLoadResult Result;
			Result.Error = Selection.Error;
			return Result;
		}
		return LoadOfflineBundle(Selection.Directory, Options);
	}
}
