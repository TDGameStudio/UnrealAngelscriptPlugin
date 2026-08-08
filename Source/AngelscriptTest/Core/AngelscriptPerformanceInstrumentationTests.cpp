#include "AngelscriptPerformanceStats.h"

#include "CQTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	static bool LoadPerformanceRuntimeSource(const TCHAR* RelativePath, FString& OutSource)
	{
		return FFileHelper::LoadFileToString(
			OutSource,
			*FPaths::Combine(
				FPaths::ProjectPluginsDir(),
				TEXT("Angelscript/Source/AngelscriptRuntime"),
				RelativePath));
	}

	static bool ExtractSourceRange(
		const FString& Source,
		const TCHAR* StartMarker,
		const TCHAR* EndMarker,
		FString& OutRange)
	{
		const int32 StartIndex = Source.Find(StartMarker);
		if (StartIndex == INDEX_NONE)
		{
			return false;
		}

		const int32 EndIndex = Source.Find(
			EndMarker,
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			StartIndex);
		if (EndIndex == INDEX_NONE || EndIndex <= StartIndex)
		{
			return false;
		}

		OutRange = Source.Mid(StartIndex, EndIndex - StartIndex);
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptPerformanceInstrumentationTests,
	"Angelscript.TestModule.Core.Performance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(InstrumentationScopeCatalog)
	{
		const TArray<FName> ScopeNames = FAngelscriptPerformanceStats::GetKnownScopeNamesForTesting();
		const TSet<FName> UniqueScopeNames(ScopeNames);
		ASSERT_THAT(AreEqual(
			UniqueScopeNames.Num(),
			ScopeNames.Num(),
			TEXT("Performance instrumentation scope catalog should not contain duplicates")));

		const FName ExpectedScopes[] = {
			TEXT("Angelscript.Startup.BindDatabase"),
			TEXT("Angelscript.Startup.BindScriptTypes"),
			TEXT("Angelscript.Binds.ExecuteCallbacks"),
			TEXT("Angelscript.Compile.Initial"),
			TEXT("Angelscript.Compile.Modules"),
			TEXT("Angelscript.Reload.HotReload"),
			TEXT("Angelscript.ClassGenerator.Setup"),
			TEXT("Angelscript.ClassGenerator.Reload"),
			TEXT("Angelscript.RuntimeCall.BPVM.JIT"),
			TEXT("Angelscript.RuntimeCall.Parms.Context"),
			TEXT("Angelscript.StaticJIT.PrecompiledData"),
			TEXT("Angelscript.DebugServer.Tick"),
			TEXT("Angelscript.Dump.All"),
			TEXT("Angelscript.Commandlet.BlueprintImpact")
		};

		for (const FName& ExpectedScope : ExpectedScopes)
		{
			const bool bContainsScope = UniqueScopeNames.Contains(ExpectedScope);
			ASSERT_THAT(IsTrue(
				bContainsScope,
				*FString::Printf(TEXT("Performance instrumentation should register scope %s"), *ExpectedScope.ToString())));
		}

		const FName RemovedOrForbiddenBindingScopes[] = {
			TEXT("Angelscript.Binds.CallBinds"),
			TEXT("Angelscript.Binds.BuildBindings"),
			TEXT("Angelscript.Binds.ApplyBindings"),
			TEXT("Angelscript.RuntimeCall.BindDispatch"),
			TEXT("Angelscript.RuntimeCall.BindingTrace"),
		};
		for (const FName& ForbiddenScope : RemovedOrForbiddenBindingScopes)
		{
			ASSERT_THAT(IsFalse(
				UniqueScopeNames.Contains(ForbiddenScope),
				*FString::Printf(
					TEXT("Performance instrumentation should not retain or introduce scope %s"),
					*ForbiddenScope.ToString())));
		}
	}

	TEST_METHOD(DirectBindingExecutionKeepsCompactProcessAndEngineState)
	{
		FString BindInternalHeader;
		FString BindsSource;
		FString EngineSource;
		FString SubsystemHeader;
		ASSERT_THAT(IsTrue(
			LoadPerformanceRuntimeSource(TEXT("Core/AngelscriptBindsInternal.h"), BindInternalHeader),
			TEXT("Structural performance guard should load AngelscriptBindsInternal.h")));
		ASSERT_THAT(IsTrue(
			LoadPerformanceRuntimeSource(TEXT("Core/AngelscriptBinds.cpp"), BindsSource),
			TEXT("Structural performance guard should load AngelscriptBinds.cpp")));
		ASSERT_THAT(IsTrue(
			LoadPerformanceRuntimeSource(TEXT("Core/AngelscriptEngine.cpp"), EngineSource),
			TEXT("Structural performance guard should load AngelscriptEngine.cpp")));
		ASSERT_THAT(IsTrue(
			LoadPerformanceRuntimeSource(TEXT("Core/AngelscriptSubsystem.h"), SubsystemHeader),
			TEXT("Structural performance guard should load AngelscriptSubsystem.h")));

		FString BindRecordDeclaration;
		ASSERT_THAT(IsTrue(
			ExtractSourceRange(
				BindInternalHeader,
				TEXT("struct FAngelscriptBindRecord"),
				TEXT("namespace UE::Angelscript::Private"),
				BindRecordDeclaration),
			TEXT("Structural performance guard should locate the process bind record")));
		ASSERT_THAT(IsTrue(
			BindRecordDeclaration.Contains(TEXT("FAngelscriptBindCallback Callback")),
			TEXT("Process bind records should retain only a direct callback pointer and stable metadata")));

		const TCHAR* ExpandedRecordTokens[] = {
			TEXT("TArray<"),
			TEXT("TMap<"),
			TEXT("TSet<"),
			TEXT("TSharedPtr"),
			TEXT("TUniquePtr"),
			TEXT("TFunction"),
			TEXT("asIScript"),
			TEXT("Declaration"),
			TEXT("FunctionId"),
			TEXT("PropertyId"),
			TEXT("TraceState"),
			TEXT("DispatchState"),
		};
		for (const TCHAR* ExpandedRecordToken : ExpandedRecordTokens)
		{
			ASSERT_THAT(IsFalse(
				BindRecordDeclaration.Contains(ExpandedRecordToken),
				*FString::Printf(
					TEXT("Process bind records should not retain expanded or per-call state '%s'"),
					ExpandedRecordToken)));
		}

		FString DirectExecutionPath;
		ASSERT_THAT(IsTrue(
			ExtractSourceRange(
				BindsSource,
				TEXT("AS_PERF_SCOPE_BINDS_EXECUTE_CALLBACKS();"),
				TEXT("OutDiagnostic.Reset();"),
				DirectExecutionPath),
			TEXT("Structural performance guard should locate direct callback execution")));
		const TCHAR* PerEngineExpansionTokens[] = {
			TEXT("TArray<"),
			TEXT("TMap<"),
			TEXT("TSet<"),
			TEXT(".Sort("),
			TEXT("Algo::Sort"),
			TEXT("GetRegisteredBindMetadata"),
			TEXT("BuildBindings"),
			TEXT("ApplyBindings"),
			TEXT("BindDispatch"),
			TEXT("BindingTrace"),
		};
		for (const TCHAR* ExpansionToken : PerEngineExpansionTokens)
		{
			ASSERT_THAT(IsFalse(
				DirectExecutionPath.Contains(ExpansionToken),
				*FString::Printf(
					TEXT("Direct callback execution should not copy, sort, expand, or dispatch through '%s'"),
					ExpansionToken)));
		}

		FString BindScriptTypesPath;
		ASSERT_THAT(IsTrue(
			ExtractSourceRange(
				EngineSource,
				TEXT("bool FAngelscriptEngine::BindScriptTypes()"),
				TEXT("void FAngelscriptEngine::FindScriptFiles"),
				BindScriptTypesPath),
			TEXT("Structural performance guard should locate BindScriptTypes")));
		ASSERT_THAT(IsTrue(
			BindScriptTypesPath.Contains(TEXT("FAngelscriptBind::ExecuteRegisteredBinds")),
			TEXT("BindScriptTypes should replay the sealed callback collection directly")));
		ASSERT_THAT(IsFalse(
			BindScriptTypesPath.Contains(TEXT("TArray<FAngelscriptBind"))
				|| BindScriptTypesPath.Contains(TEXT(".Sort("))
				|| BindScriptTypesPath.Contains(TEXT("GetRegisteredBindMetadata")),
			TEXT("BindScriptTypes should not materialize or sort a per-engine callback array")));

		const TCHAR* SubsystemBindingStateTokens[] = {
			TEXT("FAngelscriptBindRecord"),
			TEXT("FAngelscriptBindCollection"),
			TEXT("TArray<FAngelscriptBind"),
			TEXT("BindingManifest"),
			TEXT("BindOperations"),
			TEXT("BindDispatchState"),
		};
		for (const TCHAR* SubsystemBindingStateToken : SubsystemBindingStateTokens)
		{
			ASSERT_THAT(IsFalse(
				SubsystemHeader.Contains(SubsystemBindingStateToken),
				*FString::Printf(
					TEXT("Subsystems should coordinate finalization without owning '%s'"),
					SubsystemBindingStateToken)));
		}
	}
};

#endif
