#include "Compiler/AngelscriptStandaloneNativeCompiler.h"

#include "Compiler/AngelscriptStandaloneFrontend.h"
#include "Host/AngelscriptStandaloneFileSystem.h"
#include "StdLib/AngelscriptStandaloneStdLib.h"
#include "Support/AngelscriptStandaloneByteCodeStream.h"
#include "Support/AngelscriptStandaloneHash.h"

#include "UnrealAngelscriptVersion.h"
#include "angelscript.h"

#include <cstring>
#include <memory>
namespace AngelscriptStandalone
{
	namespace
	{
		struct FEngineReleaser
		{
			void operator()(asIScriptEngine* Engine) const
			{
				if (Engine != nullptr)
				{
					Engine->ShutDownAndRelease();
				}
			}
		};

		struct FThreadCleanup
		{
			~FThreadCleanup()
			{
				asThreadCleanup();
			}
		};
	}

	std::string FNativeCompiler::GetProfileHash()
	{
		const std::string Profile =
			std::string("profile=native-runtime-v1\n")
			+ "product=" UNREAL_ANGELSCRIPT_VERSION_STRING "\n"
			+ "upstream=" UNREAL_ANGELSCRIPT_UPSTREAM_LINEAGE_STRING "\n"
			+ "options=" + asGetLibraryOptions() + "\n"
			+ "addons=scriptstdstring,scriptarray,scriptdictionary,scriptmath@2.38.0-patched\n";
		return Sha256(Profile);
	}

	FCompileResult FNativeCompiler::Compile(const FCompileRequest& Request) const
	{
		FCompileResult Result;
		const FSourceResolveResult SourceResult = ResolveAndReadEntry(Request.ScriptRoots, Request.Entry);
		if (!SourceResult.bSuccess)
		{
			Result.Error = SourceResult.Error;
			return Result;
		}

		Result.LogicalEntryPath = SourceResult.Source.LogicalPath;
		const std::string InputIdentity = "source-v1\n"
			+ Result.LogicalEntryPath + "\n"
			+ SourceResult.Source.Contents;
		Result.InputHash = Sha256(InputIdentity);
		Result.ProfileHash = GetProfileHash();

		const FFrontendResult FrontendResult = ProcessNativeFrontend(SourceResult.Source);
		Result.LogicalEntryPath = FrontendResult.LogicalPath;
		Result.ModuleId = FrontendResult.ModuleId;
		if (!FrontendResult.bSuccess)
		{
			Result.Error = "source preprocessing failed";
			Result.Diagnostics = FrontendResult.Diagnostics;
			return Result;
		}

		FDiagnosticSink DiagnosticSink;
		FThreadCleanup ThreadCleanup;
		std::unique_ptr<asIScriptEngine, FEngineReleaser> Engine(asCreateScriptEngine());
		if (!Engine)
		{
			Result.Error = "asCreateScriptEngine returned null";
			return Result;
		}
		Engine->SetMessageCallback(
			asFUNCTION(FDiagnosticSink::MessageCallback),
			&DiagnosticSink,
			asCALL_CDECL);
		Engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, 1);
		Engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, 1);

		std::string RegistrationError;
		if (!RegisterNativeStandardLibrary(Engine.get(), {}, RegistrationError))
		{
			Result.Error = RegistrationError;
			Result.Diagnostics = DiagnosticSink.GetDiagnostics();
			return Result;
		}

		asIScriptModule* Module = Engine->GetModule(Result.ModuleId.c_str(), asGM_ALWAYS_CREATE);
		if (Module == nullptr
			|| Module->AddScriptSection(
				Result.LogicalEntryPath.c_str(),
				FrontendResult.ProcessedSource.data(),
				FrontendResult.ProcessedSource.size()) < 0)
		{
			Result.Error = "failed to create the module or add its source";
			Result.Diagnostics = DiagnosticSink.GetDiagnostics();
			return Result;
		}

		if (Module->Build() < 0)
		{
			Result.Error = "source compilation failed";
			Result.Diagnostics = DiagnosticSink.GetDiagnostics();
			return Result;
		}

		if (Request.bEmitByteCode)
		{
			FMemoryByteCodeStream ByteCodeStream;
			if (Module->SaveByteCode(&ByteCodeStream) < 0)
			{
				Result.Error = "bytecode generation failed";
				Result.Diagnostics = DiagnosticSink.GetDiagnostics();
				return Result;
			}
			Result.ByteCode = std::move(ByteCodeStream.Buffer);
		}

		Result.Diagnostics = DiagnosticSink.GetDiagnostics();
		Result.bSuccess = true;
		return Result;
	}
}
