#pragma once

#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

template <typename TDerived, typename TAsserter>
class TBuilderDependencySharedTestSupport : public TTest<TDerived, TAsserter>
{
protected:
	using TTest<TDerived, TAsserter>::AddInfo;

	void LogBuilderState(const FString& Stage, const asCBuilder& Builder, const asCModule* Module = nullptr, bool bExpandBuilderDescriptions = true, bool bIncludeDiagnosticCounters = true) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AddInfo(FString::Printf(TEXT("[Builder][%s] %s | %s"), *Stage, *DescribeBuilderCounts(Builder, bIncludeDiagnosticCounters), *DescribeModuleCounts(Module)));
		if (bExpandBuilderDescriptions)
		{
			AddInfo(FString::Printf(TEXT("[Builder][%s] classDecls: %s"), *Stage, *DescribeClassDeclarations(Builder)));
			AddInfo(FString::Printf(TEXT("[Builder][%s] namedTypes: %s"), *Stage, *DescribeNamedTypeDeclarations(Builder)));
			AddInfo(FString::Printf(TEXT("[Builder][%s] functionDescs: %s"), *Stage, *DescribeFunctionDescriptions(Builder)));
			AddInfo(FString::Printf(TEXT("[Builder][%s] globalDescs: %s"), *Stage, *DescribeGlobalDescriptions(Builder)));
		}
		if (Module != nullptr)
		{
			AddInfo(FString::Printf(TEXT("[Builder][%s] moduleTypes: %s"), *Stage, *DescribeModuleTypes(Module)));
			AddInfo(FString::Printf(TEXT("[Builder][%s] moduleFunctions: %s"), *Stage, *DescribeModuleFunctions(Module)));
			AddInfo(FString::Printf(TEXT("[Builder][%s] moduleGlobals: %s"), *Stage, *DescribeModuleGlobals(Module)));
		}
	}

	void LogBuilderSectionInput(const FString& Stage, const char* SectionName, const char* Source) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AddInfo(FString::Printf(
			TEXT("[Builder][%s] add section name=%s bytes=%d lines=%d"),
			*Stage,
			*ToTestString(SectionName),
			Source != nullptr ? static_cast<int32>(std::strlen(Source)) : 0,
			CountSourceLines(Source)));
	}

	void LogBuilderStageResult(const FString& Stage, int Result, const asCBuilder& Builder, const asCModule* Module = nullptr, bool bExpandBuilderDescriptions = true) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AddInfo(FString::Printf(TEXT("[Builder][%s] result=%d"), *Stage, Result));
		LogBuilderState(Stage, Builder, Module, bExpandBuilderDescriptions);
	}

	bool AddBuilderSectionWithLog(asCModule& Module, const char* SectionName, const char* Source, const FString& Stage) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		LogBuilderSectionInput(Stage, SectionName, Source);
		const bool bAdded = AddBuilderSection(Module, SectionName, Source);
		AddInfo(FString::Printf(TEXT("[Builder][%s] AddScriptSection result=%s"), *Stage, BoolText(bAdded)));
		if (Module.builder != nullptr)
		{
			LogBuilderState(Stage, *Module.builder, &Module, true, false);
		}
		else
		{
			AddInfo(FString::Printf(TEXT("[Builder][%s] %s"), *Stage, *DescribeModuleCounts(&Module)));
		}
		return bAdded;
	}

	bool RunBuilderStage(asCBuilder& Builder, const FString& Stage, int (asCBuilder::*StageMethod)(), const asCModule* Module = nullptr) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		LogBuilderState(FString::Printf(TEXT("%s.before"), *Stage), Builder, Module, true, false);
		const int Result = (Builder.*StageMethod)();
		LogBuilderStageResult(FString::Printf(TEXT("%s.after"), *Stage), Result, Builder, Module);
		return Result == asSUCCESS;
	}

	bool RunBuilderPipelineThroughLayout(asCBuilder& Builder, const asCModule* Module = nullptr) const
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		if (!RunBuilderStage(Builder, TEXT("BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module))
		{
			return false;
		}
		if (!RunBuilderStage(Builder, TEXT("BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module))
		{
			return false;
		}
		if (!RunBuilderStage(Builder, TEXT("BuildGenerateFunctions"), &asCBuilder::BuildGenerateFunctions, Module))
		{
			return false;
		}
		if (!RunBuilderStage(Builder, TEXT("BuildLayoutClasses"), &asCBuilder::BuildLayoutClasses, Module))
		{
			return false;
		}
		LogBuilderState(TEXT("BuildAllocateGlobalVariables.before"), Builder, Module, true, false);
		Builder.BuildAllocateGlobalVariables();
		LogBuilderState(TEXT("BuildAllocateGlobalVariables.after"), Builder, Module);
		return RunBuilderStage(Builder, TEXT("BuildLayoutFunctions"), &asCBuilder::BuildLayoutFunctions, Module);
	}
};

#endif
