#include "Runtime/AngelscriptStandaloneRunner.h"

#include "Compiler/AngelscriptStandaloneFrontend.h"
#include "Compiler/AngelscriptStandaloneNativeCompiler.h"
#include "Host/AngelscriptStandaloneFileSystem.h"
#include "Runtime/AngelscriptStandaloneAllocator.h"
#include "Runtime/AngelscriptStandaloneExecutionGuard.h"
#include "StdLib/AngelscriptStandaloneStdLib.h"
#include "Support/AngelscriptStandaloneByteCodeStream.h"
#include "Support/AngelscriptStandaloneHash.h"

#include "angelscript.h"
#include "scriptarray/scriptarray.h"
#include "scriptstdstring/scriptstdstring.h"

#include <memory>
#include <new>

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

		struct FContextReleaser
		{
			void operator()(asIScriptContext* Context) const
			{
				if (Context != nullptr)
				{
					Context->Release();
				}
			}
		};

		void CaptureCallStack(asIScriptContext* Context, FRunResult& Result)
		{
			for (asUINT Index = 0; Index < Context->GetCallstackSize(); ++Index)
			{
				FCallStackFrame Frame;
				asIScriptFunction* Function = Context->GetFunction(Index);
				if (Function != nullptr)
				{
					Frame.Declaration = Function->GetDeclaration();
				}
				const char* Section = nullptr;
				Frame.Row = Context->GetLineNumber(Index, &Frame.Column, &Section);
				Frame.Section = Section != nullptr ? Section : "";
				Result.CallStack.push_back(std::move(Frame));
			}
		}

		void AddRuntimeDiagnostic(
			FRunResult& Result,
			const char* Code,
			EDiagnosticSeverity Severity,
			const std::string& Message)
		{
			FDiagnostic Diagnostic;
			Diagnostic.Code = Code;
			Diagnostic.Severity = Severity;
			Diagnostic.Message = Message;
			Result.Diagnostics.push_back(std::move(Diagnostic));
		}

		void FinalizeRuntime(
			std::unique_ptr<asIScriptEngine, FEngineReleaser>& Engine,
			FCountingAllocator& Allocator,
			FRunResult& Result)
		{
			Engine.reset();
			asThreadCleanup();
			Result.PeakAllocatedBytes = Allocator.GetPeakBytes();
			Result.AllocatedBytesAfterShutdown = Allocator.GetCurrentBytes();
			Allocator.Uninstall();
		}
	}

	FRunResult RunNativeScript(const FRunRequest& Request)
	{
		FRunResult Result;
		const FSourceResolveResult SourceResult = ResolveAndReadEntry(Request.ScriptRoots, Request.Entry);
		if (!SourceResult.bSuccess)
		{
			Result.Error = SourceResult.Error;
			return Result;
		}
		Result.LogicalEntryPath = SourceResult.Source.LogicalPath;
		Result.InputHash = Sha256(
			"source-v1\n" + Result.LogicalEntryPath + "\n" + SourceResult.Source.Contents);
		Result.ProfileHash = FNativeCompiler::GetProfileHash();

		const FFrontendResult FrontendResult = ProcessNativeFrontend(SourceResult.Source);
		Result.LogicalEntryPath = FrontendResult.LogicalPath;
		Result.ModuleId = FrontendResult.ModuleId;
		if (!FrontendResult.bSuccess)
		{
			Result.ExitCode = 1;
			Result.Error = "source preprocessing failed";
			Result.Diagnostics = FrontendResult.Diagnostics;
			return Result;
		}
		if (Request.MemoryLimitBytes < MinimumNativeRunMemoryBytes)
		{
			Result.ExitCode = 4;
			Result.bMemoryLimitReached = true;
			Result.Error =
				"native execution memory limit is below the 16 MiB "
				"minimum engine bootstrap budget";
			AddRuntimeDiagnostic(
				Result,
				"AS-NATIVE-MEMORY-LIMIT",
				EDiagnosticSeverity::Error,
				Result.Error);
			return Result;
		}

		FCountingAllocator Allocator(Request.MemoryLimitBytes);
		if (!Allocator.Install())
		{
			Result.Error = "failed to install the standalone counting allocator";
			return Result;
		}

		FDiagnosticSink DiagnosticSink;
		std::unique_ptr<asIScriptEngine, FEngineReleaser> Engine(asCreateScriptEngine());
		if (!Engine)
		{
			Result.Error = Allocator.RejectedAnyAllocation()
				? "memory limit prevented engine creation"
				: "asCreateScriptEngine returned null";
			Result.bMemoryLimitReached = Allocator.RejectedAnyAllocation();
			Result.ExitCode = Result.bMemoryLimitReached ? 4 : 2;
			FinalizeRuntime(Engine, Allocator, Result);
			return Result;
		}
		if (Allocator.RejectedAnyAllocation())
		{
			Result.ExitCode = 4;
			Result.bMemoryLimitReached = true;
			Result.Error = "memory limit reached during engine creation";
			AddRuntimeDiagnostic(
				Result,
				"AS-NATIVE-MEMORY-LIMIT",
				EDiagnosticSeverity::Error,
				Result.Error);
			FinalizeRuntime(Engine, Allocator, Result);
			return Result;
		}

		Engine->SetMessageCallback(
			asFUNCTION(FDiagnosticSink::MessageCallback),
			&DiagnosticSink,
			asCALL_CDECL);
		Engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, 1);
		Engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, 1);

		FStandardLibraryOptions StandardLibraryOptions;
		StandardLibraryOptions.Print = [&Result](const std::string& Line)
		{
			Result.StandardOutput += Line;
			Result.StandardOutput.push_back('\n');
		};
		std::string RegistrationError;
		bool bRegistrationSucceeded = false;
		try
		{
			bRegistrationSucceeded = RegisterNativeStandardLibrary(
				Engine.get(),
				StandardLibraryOptions,
				RegistrationError);
		}
		catch (const std::bad_alloc&)
		{
			RegistrationError = "memory limit reached during standard-library registration";
		}
		if (!bRegistrationSucceeded)
		{
			Result.bMemoryLimitReached = Allocator.RejectedAnyAllocation();
			Result.ExitCode = Result.bMemoryLimitReached ? 4 : 2;
			Result.Error = RegistrationError;
			Result.Diagnostics = DiagnosticSink.GetDiagnostics();
			if (Result.bMemoryLimitReached)
			{
				AddRuntimeDiagnostic(
					Result,
					"AS-NATIVE-MEMORY-LIMIT",
					EDiagnosticSeverity::Error,
					Result.Error);
			}
			FinalizeRuntime(Engine, Allocator, Result);
			return Result;
		}
		if (Allocator.RejectedAnyAllocation())
		{
			Result.ExitCode = 4;
			Result.bMemoryLimitReached = true;
			Result.Error = "memory limit reached during standard-library registration";
			Result.Diagnostics = DiagnosticSink.GetDiagnostics();
			AddRuntimeDiagnostic(
				Result,
				"AS-NATIVE-MEMORY-LIMIT",
				EDiagnosticSeverity::Error,
				Result.Error);
			FinalizeRuntime(Engine, Allocator, Result);
			return Result;
		}

		asIScriptModule* Module = Engine->GetModule(Result.ModuleId.c_str(), asGM_ALWAYS_CREATE);
		const bool bModuleSetupFailed =
			Module == nullptr
			|| Module->AddScriptSection(
				FrontendResult.LogicalPath.c_str(),
				FrontendResult.ProcessedSource.data(),
				FrontendResult.ProcessedSource.size()) < 0;
		bool bBuildAllocationFailed = false;
		bool bBuildFailed = false;
		if (!bModuleSetupFailed)
		{
			try
			{
				bBuildFailed = Module->Build() < 0;
			}
			catch (const std::bad_alloc&)
			{
				bBuildAllocationFailed = true;
			}
		}
		if (bModuleSetupFailed
			|| bBuildFailed
			|| bBuildAllocationFailed
			|| Allocator.RejectedAnyAllocation())
		{
			Result.ExitCode = Allocator.RejectedAnyAllocation() ? 4 : 1;
			Result.bMemoryLimitReached = Allocator.RejectedAnyAllocation();
			Result.Error = Result.bMemoryLimitReached
				? "memory limit reached during compilation"
				: "source compilation failed";
			Result.Diagnostics = DiagnosticSink.GetDiagnostics();
			if (Result.bMemoryLimitReached)
			{
				AddRuntimeDiagnostic(
					Result,
					"AS-NATIVE-MEMORY-LIMIT",
					EDiagnosticSeverity::Error,
					Result.Error);
			}
			FinalizeRuntime(Engine, Allocator, Result);
			return Result;
		}

		FMemoryByteCodeStream ByteCodeStream;
		if (Module->SaveByteCode(&ByteCodeStream) < 0)
		{
			Result.ExitCode = 1;
			Result.Error = "bytecode generation failed";
			Result.Diagnostics = DiagnosticSink.GetDiagnostics();
			AddRuntimeDiagnostic(
				Result,
				"AS-NATIVE-BYTECODE",
				EDiagnosticSeverity::Error,
				Result.Error);
			FinalizeRuntime(Engine, Allocator, Result);
			return Result;
		}
		if (Allocator.RejectedAnyAllocation())
		{
			Result.ExitCode = 4;
			Result.bMemoryLimitReached = true;
			Result.Error = "memory limit reached during bytecode generation";
			Result.Diagnostics = DiagnosticSink.GetDiagnostics();
			AddRuntimeDiagnostic(
				Result,
				"AS-NATIVE-MEMORY-LIMIT",
				EDiagnosticSeverity::Error,
				Result.Error);
			FinalizeRuntime(Engine, Allocator, Result);
			return Result;
		}
		Result.ByteCode = std::move(ByteCodeStream.Buffer);

		asIScriptFunction* IntMain =
			Module->GetFunctionByDecl("int main(const array<string> args)");
		asIScriptFunction* VoidMain =
			Module->GetFunctionByDecl("void main(const array<string> args)");
		if ((IntMain == nullptr) == (VoidMain == nullptr))
		{
			Result.ExitCode = 1;
			Result.Error =
				"exactly one entry is required: int main(const array<string> args) "
				"or void main(const array<string> args)";
			Result.Diagnostics = DiagnosticSink.GetDiagnostics();
			AddRuntimeDiagnostic(
				Result,
				"AS-NATIVE-ENTRY",
				EDiagnosticSeverity::Error,
				Result.Error);
			FinalizeRuntime(Engine, Allocator, Result);
			return Result;
		}

		asIScriptFunction* Entry = IntMain != nullptr ? IntMain : VoidMain;
		std::unique_ptr<asIScriptContext, FContextReleaser> Context(Engine->CreateContext());
		asITypeInfo* ArgumentArrayType = Engine->GetTypeInfoByDecl("array<string>");
		CScriptArray* ArgumentArray =
			ArgumentArrayType != nullptr
				? CScriptArray::Create(ArgumentArrayType, static_cast<asUINT>(Request.Arguments.size()))
				: nullptr;
		if (!Context || ArgumentArray == nullptr)
		{
			if (ArgumentArray != nullptr)
			{
				ArgumentArray->Release();
			}
			Result.ExitCode = Allocator.RejectedAnyAllocation() ? 4 : 2;
			Result.bMemoryLimitReached = Allocator.RejectedAnyAllocation();
			Result.Error = "failed to create the execution context or argument array";
			Result.Diagnostics = DiagnosticSink.GetDiagnostics();
			AddRuntimeDiagnostic(
				Result,
				"AS-NATIVE-RUNTIME-INFRASTRUCTURE",
				EDiagnosticSeverity::Error,
				Result.Error);
			Context.reset();
			FinalizeRuntime(Engine, Allocator, Result);
			return Result;
		}

		bool bArgumentAllocationFailed = false;
		try
		{
			for (asUINT Index = 0; Index < Request.Arguments.size(); ++Index)
			{
				auto* Argument = static_cast<scriptstring_t*>(ArgumentArray->At(Index));
				Argument->assign(
					Request.Arguments[Index].data(),
					Request.Arguments[Index].size());
			}
		}
		catch (const std::bad_alloc&)
		{
			bArgumentAllocationFailed = true;
		}
		if (bArgumentAllocationFailed || Allocator.RejectedAnyAllocation())
		{
			ArgumentArray->Release();
			Result.ExitCode = 4;
			Result.bMemoryLimitReached = true;
			Result.Error = "native argument materialization reached the memory limit";
			Result.Diagnostics = DiagnosticSink.GetDiagnostics();
			AddRuntimeDiagnostic(
				Result,
				"AS-NATIVE-MEMORY-LIMIT",
				EDiagnosticSeverity::Error,
				Result.Error);
			Context.reset();
			FinalizeRuntime(Engine, Allocator, Result);
			return Result;
		}

		FExecutionGuard ExecutionGuard(Request.TimeoutMilliseconds, &Allocator);
		const int PrepareResult = Context->Prepare(Entry);
		if (PrepareResult >= 0)
		{
			// SetArgObject stores the pointer without taking a reference. The
			// by-value array argument releases its reference when execution or
			// context cleanup finishes, so retain a separate host-owned reference.
			ArgumentArray->AddRef();
		}
		const int ArgumentResult = PrepareResult >= 0
			? Context->SetArgObject(0, ArgumentArray)
			: PrepareResult;
		if (PrepareResult >= 0 && ArgumentResult < 0)
		{
			ArgumentArray->Release();
		}
		const int CallbackResult = ArgumentResult >= 0
			? Context->SetInstructionCallback(
				&FExecutionGuard::InstructionCallback,
				&ExecutionGuard)
			: ArgumentResult;
		int ExecutionResult = CallbackResult;
		if (CallbackResult >= 0)
		{
			try
			{
				ExecutionResult = Context->Execute();
			}
			catch (const std::bad_alloc&)
			{
				Context->Abort();
				ExecutionResult = asEXECUTION_ABORTED;
			}
		}

		if (ExecutionResult == asEXECUTION_FINISHED)
		{
			Result.ExitCode = 0;
			if (IntMain != nullptr)
			{
				Result.ScriptResult = static_cast<int>(Context->GetReturnDWord());
			}
		}
		else if (ExecutionGuard.IsTimedOut() || Allocator.RejectedAnyAllocation())
		{
			Result.ExitCode = 4;
			Result.bTimedOut = ExecutionGuard.IsTimedOut();
			Result.bMemoryLimitReached = Allocator.RejectedAnyAllocation();
			Result.Error = Result.bTimedOut
				? "native execution timed out"
				: "native execution reached the memory limit";
			AddRuntimeDiagnostic(
				Result,
				Result.bTimedOut ? "AS-NATIVE-TIMEOUT" : "AS-NATIVE-MEMORY-LIMIT",
				EDiagnosticSeverity::Error,
				Result.Error);
		}
		else if (ExecutionResult == asEXECUTION_EXCEPTION)
		{
			Result.ExitCode = 3;
			Result.ExceptionMessage = Context->GetExceptionString() != nullptr
				? Context->GetExceptionString()
				: "script exception";
			CaptureCallStack(Context.get(), Result);
			AddRuntimeDiagnostic(
				Result,
				"AS-NATIVE-EXCEPTION",
				EDiagnosticSeverity::Error,
				Result.ExceptionMessage);
		}
		else
		{
			Result.ExitCode = 3;
			Result.Error = "native execution aborted";
			CaptureCallStack(Context.get(), Result);
			AddRuntimeDiagnostic(
				Result,
				"AS-NATIVE-ABORT",
				EDiagnosticSeverity::Error,
				Result.Error);
		}
		ArgumentArray->Release();
		const std::vector<FDiagnostic>& CompilerDiagnostics = DiagnosticSink.GetDiagnostics();
		Result.Diagnostics.insert(
			Result.Diagnostics.begin(),
			CompilerDiagnostics.begin(),
			CompilerDiagnostics.end());
		Context.reset();
		FinalizeRuntime(Engine, Allocator, Result);
		return Result;
	}
}
