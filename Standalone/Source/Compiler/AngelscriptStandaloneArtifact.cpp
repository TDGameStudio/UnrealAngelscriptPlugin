#include "Compiler/AngelscriptStandaloneArtifact.h"

#include "Support/AngelscriptStandaloneHash.h"
#include "Support/AngelscriptStandaloneJson.h"

#include "UnrealAngelscriptVersion.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

namespace AngelscriptStandalone
{
	namespace
	{
		struct FDiagnosticCounts
		{
			int Errors = 0;
			int Warnings = 0;
			int Infos = 0;
		};

		FDiagnosticCounts CountDiagnostics(const std::vector<FDiagnostic>& Diagnostics)
		{
			FDiagnosticCounts Counts;
			for (const FDiagnostic& Diagnostic : Diagnostics)
			{
				Counts.Errors += Diagnostic.Severity == EDiagnosticSeverity::Error ? 1 : 0;
				Counts.Warnings += Diagnostic.Severity == EDiagnosticSeverity::Warning ? 1 : 0;
				Counts.Infos += Diagnostic.Severity == EDiagnosticSeverity::Info ? 1 : 0;
			}
			return Counts;
		}

		bool WriteText(const std::filesystem::path& Path, const std::string& Text)
		{
			std::ofstream Output(Path, std::ios::binary | std::ios::trunc);
			Output.write(Text.data(), static_cast<std::streamsize>(Text.size()));
			return Output.good();
		}

		bool WriteBinary(const std::filesystem::path& Path, const std::vector<std::uint8_t>& Bytes)
		{
			std::ofstream Output(Path, std::ios::binary | std::ios::trunc);
			Output.write(
				reinterpret_cast<const char*>(Bytes.data()),
				static_cast<std::streamsize>(Bytes.size()));
			return Output.good();
		}

		bool RenameArtifactDirectory(
			const std::filesystem::path& Source,
			const std::filesystem::path& Destination,
			std::error_code& Error)
		{
			constexpr int MaximumAttempts = 25;
			constexpr auto RetryDelay = std::chrono::milliseconds(10);
			for (int Attempt = 0; Attempt < MaximumAttempts; ++Attempt)
			{
				Error.clear();
				std::filesystem::rename(Source, Destination, Error);
				if (!Error)
				{
					return true;
				}

				const bool bRetryable =
					Error == std::errc::permission_denied
					|| Error == std::errc::device_or_resource_busy
					|| Error == std::errc::resource_unavailable_try_again;
				if (!bRetryable || Attempt + 1 == MaximumAttempts)
				{
					return false;
				}
				std::this_thread::sleep_for(RetryDelay);
			}
			return false;
		}

		void AppendDiagnosticCounts(
			std::ostringstream& Json,
			const std::vector<FDiagnostic>& Diagnostics)
		{
			const FDiagnosticCounts Counts = CountDiagnostics(Diagnostics);
			Json
				<< "\"diagnosticCounts\": {\"error\": " << Counts.Errors
				<< ", \"info\": " << Counts.Infos
				<< ", \"warning\": " << Counts.Warnings << "}";
		}

		void AppendModule(
			std::ostringstream& Json,
			const std::string& ModuleId,
			const std::vector<std::uint8_t>& ByteCode)
		{
			Json << "\"module\": {\"bytecodeHash\": ";
			if (ByteCode.empty())
			{
				Json << "null, \"bytecodePath\": null";
			}
			else
			{
				Json
					<< EscapeJsonString(Sha256(ByteCode))
					<< ", \"bytecodePath\": "
					<< EscapeJsonString("modules/" + ModuleId + ".asbc");
			}
			Json << ", \"id\": " << EscapeJsonString(ModuleId) << "}";
		}

		std::string MakeCompileResultJson(const FCompileResult& Result)
		{
			std::ostringstream Json;
			Json
				<< "{\n"
				<< "  \"compiler\": "
				<< EscapeJsonString(UNREAL_ANGELSCRIPT_PRODUCT_VERSION_STRING) << ",\n"
				<< "  ";
			AppendDiagnosticCounts(Json, Result.Diagnostics);
			Json
				<< ",\n"
				<< "  \"inputHash\": " << EscapeJsonString(Result.InputHash) << ",\n"
				<< "  \"logicalEntry\": " << EscapeJsonString(Result.LogicalEntryPath) << ",\n"
				<< "  ";
			AppendModule(Json, Result.ModuleId, Result.ByteCode);
			Json
				<< ",\n"
				<< "  \"profile\": \"native-runtime\",\n"
				<< "  \"profileHash\": " << EscapeJsonString(Result.ProfileHash) << ",\n"
				<< "  \"schema\": \"angelscript-standalone-result/1.0\",\n"
				<< "  \"status\": " << EscapeJsonString(Result.bSuccess ? "complete" : "failed")
				<< ",\n"
				<< "  \"upstreamLineage\": "
				<< EscapeJsonString(UNREAL_ANGELSCRIPT_UPSTREAM_LINEAGE_STRING) << "\n"
				<< "}\n";
			return Json.str();
		}

		const char* GetExecutionStatus(const FRunResult& Result)
		{
			if (Result.ExitCode == 0)
			{
				return "finished";
			}
			if (Result.bTimedOut)
			{
				return "timed-out";
			}
			if (Result.bMemoryLimitReached)
			{
				return "memory-limit";
			}
			if (!Result.ExceptionMessage.empty())
			{
				return "exception";
			}
			if (Result.ExitCode == 1)
			{
				return "source-or-entry-failed";
			}
			if (Result.ExitCode == 3)
			{
				return "aborted";
			}
			return "infrastructure-failed";
		}

		std::string MakeRunResultJson(const FRunResult& Result)
		{
			std::ostringstream Json;
			Json
				<< "{\n"
				<< "  \"compiler\": "
				<< EscapeJsonString(UNREAL_ANGELSCRIPT_PRODUCT_VERSION_STRING) << ",\n"
				<< "  ";
			AppendDiagnosticCounts(Json, Result.Diagnostics);
			Json
				<< ",\n"
				<< "  \"error\": "
				<< (Result.Error.empty() ? "null" : EscapeJsonString(Result.Error)) << ",\n"
				<< "  \"exception\": "
				<< (Result.ExceptionMessage.empty()
					? "null"
					: EscapeJsonString(Result.ExceptionMessage))
				<< ",\n"
				<< "  \"executionStatus\": "
				<< EscapeJsonString(GetExecutionStatus(Result)) << ",\n"
				<< "  \"inputHash\": " << EscapeJsonString(Result.InputHash) << ",\n"
				<< "  \"logicalEntry\": " << EscapeJsonString(Result.LogicalEntryPath) << ",\n"
				<< "  ";
			AppendModule(Json, Result.ModuleId, Result.ByteCode);
			Json
				<< ",\n"
				<< "  \"profile\": \"native-runtime\",\n"
				<< "  \"profileHash\": " << EscapeJsonString(Result.ProfileHash) << ",\n"
				<< "  \"resourceUsage\": {\"allocatedBytesAfterShutdown\": "
				<< Result.AllocatedBytesAfterShutdown
				<< ", \"memoryLimitReached\": "
				<< (Result.bMemoryLimitReached ? "true" : "false")
				<< ", \"peakAllocatedBytes\": " << Result.PeakAllocatedBytes
				<< ", \"timedOut\": " << (Result.bTimedOut ? "true" : "false")
				<< "},\n"
				<< "  \"schema\": \"angelscript-standalone-result/1.0\",\n"
				<< "  \"scriptResult\": ";
			if (Result.ScriptResult.has_value())
			{
				Json << *Result.ScriptResult;
			}
			else
			{
				Json << "null";
			}
			Json
				<< ",\n"
				<< "  \"status\": "
				<< EscapeJsonString(Result.ExitCode == 0 ? "complete" : "failed") << ",\n"
				<< "  \"upstreamLineage\": "
				<< EscapeJsonString(UNREAL_ANGELSCRIPT_UPSTREAM_LINEAGE_STRING) << "\n"
				<< "}\n";
			return Json.str();
		}

		std::string MakeDiagnosticsJsonLines(const std::vector<FDiagnostic>& Diagnostics)
		{
			std::ostringstream JsonLines;
			for (const FDiagnostic& Diagnostic : Diagnostics)
			{
				JsonLines
					<< "{\"code\":" << EscapeJsonString(Diagnostic.Code)
					<< ",\"column\":" << Diagnostic.Column
					<< ",\"message\":" << EscapeJsonString(Diagnostic.Message)
					<< ",\"row\":" << Diagnostic.Row
					<< ",\"section\":" << EscapeJsonString(Diagnostic.Section)
					<< ",\"severity\":" << EscapeJsonString(ToString(Diagnostic.Severity))
					<< "}\n";
			}
			return JsonLines.str();
		}

		FArtifactWriteResult PublishArtifacts(
			const std::filesystem::path& OutputDirectory,
			const std::string& ResultJson,
			const std::vector<FDiagnostic>& Diagnostics,
			const std::string& ModuleId,
			const std::vector<std::uint8_t>& ByteCode)
		{
			FArtifactWriteResult Result;
			if (OutputDirectory.empty())
			{
				Result.Error = "artifact request requires an output directory";
				return Result;
			}

			const std::filesystem::path Staging =
				OutputDirectory.parent_path()
				/ (OutputDirectory.filename().string() + ".staging");
			const std::filesystem::path Backup =
				OutputDirectory.parent_path()
				/ (OutputDirectory.filename().string() + ".previous");
			std::error_code Error;
			std::filesystem::remove_all(Staging, Error);
			Error.clear();
			std::filesystem::create_directories(Staging / "modules", Error);
			if (Error)
			{
				Result.Error = "failed to create artifact staging directory";
				return Result;
			}

			if (!WriteText(Staging / "result.json", ResultJson)
				|| !WriteText(
					Staging / "diagnostics.jsonl",
					MakeDiagnosticsJsonLines(Diagnostics))
				|| (!ByteCode.empty()
					&& (ModuleId.empty()
						|| !WriteBinary(
							Staging / "modules" / (ModuleId + ".asbc"),
							ByteCode))))
			{
				std::filesystem::remove_all(Staging, Error);
				Result.Error = "failed to write artifacts";
				return Result;
			}

			std::filesystem::remove_all(Backup, Error);
			Error.clear();
			if (std::filesystem::exists(OutputDirectory))
			{
				if (!RenameArtifactDirectory(OutputDirectory, Backup, Error))
				{
					std::filesystem::remove_all(Staging, Error);
					Result.Error = "failed to prepare atomic artifact replacement";
					return Result;
				}
			}

			if (!RenameArtifactDirectory(Staging, OutputDirectory, Error))
			{
				const std::string PublishError = Error.message();
				std::error_code RollbackError;
				if (std::filesystem::exists(Backup))
				{
					RenameArtifactDirectory(
						Backup,
						OutputDirectory,
						RollbackError);
				}
				std::filesystem::remove_all(Staging, RollbackError);
				Result.Error = "failed to publish artifacts: " + PublishError;
				return Result;
			}

			std::filesystem::remove_all(Backup, Error);
			Result.bSuccess = true;
			return Result;
		}
	}

	FArtifactWriteResult WriteCompileArtifacts(const FArtifactWriteRequest& Request)
	{
		if (Request.CompileResult == nullptr)
		{
			return {false, "artifact request requires a compile result"};
		}
		return PublishArtifacts(
			Request.OutputDirectory,
			MakeCompileResultJson(*Request.CompileResult),
			Request.CompileResult->Diagnostics,
			Request.CompileResult->ModuleId,
			Request.CompileResult->ByteCode);
	}

	FArtifactWriteResult WriteRunArtifacts(const FRunArtifactWriteRequest& Request)
	{
		if (Request.RunResult == nullptr)
		{
			return {false, "artifact request requires a run result"};
		}
		return PublishArtifacts(
			Request.OutputDirectory,
			MakeRunResultJson(*Request.RunResult),
			Request.RunResult->Diagnostics,
			Request.RunResult->ModuleId,
			Request.RunResult->ByteCode);
	}
}
