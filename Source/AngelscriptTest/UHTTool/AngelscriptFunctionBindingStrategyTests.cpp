#include "CQTest.h"

#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Features/IModularFeatures.h"
#include "FunctionBinding/NativeModuleFunctionBindingBridge.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include <cstddef>
#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	static FString GetPluginDirectory()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("Angelscript"));
	}

	static FString GetCompileOptionsPath()
	{
		return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultAngelscriptCompileOptions.ini"));
	}

	static FString GetGeneratedDirectory()
	{
		return FPaths::Combine(GetPluginDirectory(), TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));
	}

	static bool ReadFile(const FString& Path, FString& Contents, FAutomationTestBase& Test)
	{
		return Test.TestTrue(*FString::Printf(TEXT("Expected file to be readable: %s"), *Path), FFileHelper::LoadFileToString(Contents, *Path));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptFunctionBindingStrategyContractTests,
	"Angelscript.CppTests.UHTToolResolver.FunctionBindingStrategy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RuntimeLinkedEmitterUsesExplicitGeneratedCallback)
	{
		FString EmitterContents;
		FString ModelContents;
		const FString EmitterPath = FPaths::Combine(
			GetPluginDirectory(),
			TEXT("Source/AngelscriptUHTTool/AngelscriptFunctionBindingEmitters.cs"));
		const FString ModelPath = FPaths::Combine(
			GetPluginDirectory(),
			TEXT("Source/AngelscriptUHTTool/AngelscriptFunctionBindingModels.cs"));

		ASSERT_THAT(IsTrue(
			FFileHelper::LoadFileToString(EmitterContents, *EmitterPath),
			*FString::Printf(TEXT("Runtime-linked emitter should be readable: %s"), *EmitterPath)));
		ASSERT_THAT(IsTrue(
			FFileHelper::LoadFileToString(ModelContents, *ModelPath),
			*FString::Printf(TEXT("Runtime-linked model should be readable: %s"), *ModelPath)));

		ASSERT_THAT(IsTrue(
			EmitterContents.Contains(TEXT("MaxBindingsPerRuntimeLinkedHelperFunction = 256")),
			TEXT("Runtime-linked emitter should preserve 256-entry batches")));
		ASSERT_THAT(IsTrue(
			EmitterContents.Contains(TEXT("BindGeneratedFunctionBindings_"))
				&& EmitterContents.Contains(TEXT("FAngelscriptBinds& Binds"))
				&& EmitterContents.Contains(TEXT("EAngelscriptBindPhase::GeneratedBindings")),
			TEXT("Runtime-linked emitter should build one named explicit GeneratedBindings callback")));
		ASSERT_THAT(IsTrue(
			ModelContents.Contains(TEXT("Binds.RegisterGeneratedFunctionBindingForTarget")),
			TEXT("Runtime-linked model should emit explicit generated registrations")));
		ASSERT_THAT(IsFalse(
			EmitterContents.Contains(TEXT("FAngelscriptBinds::FBind"))
				|| EmitterContents.Contains(TEXT("FAngelscriptBinds::EOrder"))
				|| ModelContents.Contains(TEXT("FAngelscriptBinds::RegisterGeneratedFunctionBinding(")),
			TEXT("Runtime-linked generator sources should not retain legacy provider syntax")));
	}

	TEST_METHOD(NativeModuleEmitterPreservesTargetModuleIndependenceAndBridgeLayout)
	{
		FString EmitterContents;
		FString BridgeContents;
		FString LayoutVersionContents;
		const FString EmitterPath = FPaths::Combine(
			GetPluginDirectory(),
			TEXT("Source/AngelscriptUHTTool/AngelscriptFunctionBindingEmitters.cs"));
		const FString BridgePath = FPaths::Combine(
			GetPluginDirectory(),
			TEXT("Source/AngelscriptRuntime/FunctionBinding/NativeModuleFunctionBindingBridge.h"));
		const FString LayoutVersionPath = FPaths::Combine(
			GetPluginDirectory(),
			TEXT("Source/AngelscriptUHTTool/native-module-function-binding-layout-version.txt"));

		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(EmitterContents, *EmitterPath)));
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(BridgeContents, *BridgePath)));
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(LayoutVersionContents, *LayoutVersionPath)));

		const int32 TargetShardStart = EmitterContents.Find(
			TEXT("private static StringBuilder BuildNativeModuleFunctionAddressShard"),
			ESearchCase::CaseSensitive);
		const int32 TargetShardEnd = EmitterContents.Find(
			TEXT("private static string BuildNativeModuleFunctionBindingThunkName"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			TargetShardStart + 1);
		ASSERT_THAT(IsTrue(
			TargetShardStart != INDEX_NONE && TargetShardEnd > TargetShardStart,
			TEXT("NativeModuleFunctionAddress shard emitter section should remain discoverable")));

		const FString TargetShardEmitter = EmitterContents.Mid(
			TargetShardStart,
			TargetShardEnd - TargetShardStart);
		ASSERT_THAT(IsTrue(
			TargetShardEmitter.Contains(TEXT("#include \\\"Features/IModularFeatures.h\\\""))
				&& TargetShardEmitter.Contains(TEXT("RegisterModularFeature"))
				&& TargetShardEmitter.Contains(TEXT("UnregisterModularFeature")),
			TEXT("Target-module shards should preserve modular-feature arrival and unload transport")));
		ASSERT_THAT(IsTrue(
			TargetShardEmitter.Contains(TEXT("sizeof(FAngelscriptNativeModuleFunctionBindingCallFrame) == 48"))
				&& TargetShardEmitter.Contains(TEXT("sizeof(FAngelscriptNativeModuleFunctionBinding) == 32"))
				&& TargetShardEmitter.Contains(TEXT("sizeof(FAngelscriptNativeModuleFunctionBindingView) == 32")),
			TEXT("Target-module shards should preserve the callframe, POD, and view ABI guards")));
		ASSERT_THAT(IsFalse(
			TargetShardEmitter.Contains(TEXT("AngelscriptBinds"))
				|| TargetShardEmitter.Contains(TEXT("AngelscriptRuntime"))
				|| TargetShardEmitter.Contains(TEXT("NativeModuleFunctionBindingBridge.h")),
			TEXT("Target-module shards should remain independent of AngelscriptRuntime")));
		ASSERT_THAT(IsTrue(
			LayoutVersionContents.Contains(TEXT("0xA5C0DE02"))
				&& BridgeContents.Contains(TEXT("0xA5C0DE02u")),
			TEXT("Runtime bridge and target emitter should retain the current layout-version token")));
	}

	TEST_METHOD(CompileOptionsUseTheGlobalFunctionBindingMethod)
	{
		FString HeaderContents;
		FString ConfigContents;
		FString BuildCsContents;
		FString GeneratorContents;
		FString ConfigurationContents;
		FString PolicyContents;
		FString ResolverContents;
		FString EditorContents;
		bool bPassed = true;
		bPassed &= ReadFile(FPaths::Combine(GetPluginDirectory(), TEXT("Source/AngelscriptRuntime/Core/AngelscriptCompileOptions.h")), HeaderContents, *TestRunner);
		bPassed &= ReadFile(GetCompileOptionsPath(), ConfigContents, *TestRunner);
		bPassed &= ReadFile(FPaths::Combine(GetPluginDirectory(), TEXT("Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs")), BuildCsContents, *TestRunner);
		bPassed &= ReadFile(FPaths::Combine(GetPluginDirectory(), TEXT("Source/AngelscriptUHTTool/AngelscriptFunctionBindingCodeGenerator.cs")), GeneratorContents, *TestRunner);
		bPassed &= ReadFile(FPaths::Combine(GetPluginDirectory(), TEXT("Source/AngelscriptUHTTool/AngelscriptFunctionBindingConfiguration.cs")), ConfigurationContents, *TestRunner);
		bPassed &= ReadFile(FPaths::Combine(GetPluginDirectory(), TEXT("Source/AngelscriptUHTTool/AngelscriptFunctionBindingPolicy.cs")), PolicyContents, *TestRunner);
		bPassed &= ReadFile(FPaths::Combine(GetPluginDirectory(), TEXT("Source/AngelscriptUHTTool/AngelscriptFunctionBindingConfigurationResolver.cs")), ResolverContents, *TestRunner);
		bPassed &= ReadFile(FPaths::Combine(GetPluginDirectory(), TEXT("Source/AngelscriptEditor/Core/AngelscriptEditorModule.cpp")), EditorContents, *TestRunner);
		if (!bPassed)
		{
			return;
		}

		bPassed &= TestRunner->TestTrue(TEXT("Compile options should declare the UE binding method enum"), HeaderContents.Contains(TEXT("EAngelscriptFunctionBindingMethod")));
		bPassed &= TestRunner->TestTrue(TEXT("Compile options should expose the global binding method"), HeaderContents.Contains(TEXT("FunctionBindingMethod")));
		bPassed &= TestRunner->TestTrue(TEXT("Compile options should expose Runtime-linked module arrays"), HeaderContents.Contains(TEXT("NativeRuntimeLinkedModules")));
		bPassed &= TestRunner->TestTrue(TEXT("Compile options should expose target-module arrays"), HeaderContents.Contains(TEXT("NativeModuleFunctionAddressModules")));
		bPassed &= TestRunner->TestTrue(TEXT("Default config should select NativeRuntimeLinked"), ConfigContents.Contains(TEXT("FunctionBindingMethod=NativeRuntimeLinked")));
		bPassed &= TestRunner->TestTrue(TEXT("Default config should use UE array syntax"), ConfigContents.Contains(TEXT("+NativeRuntimeLinkedModules=")));
		bPassed &= TestRunner->TestTrue(TEXT("Build.cs should parse FunctionBindingMethod"), BuildCsContents.Contains(TEXT("FunctionBindingMethod")));
		bPassed &= TestRunner->TestTrue(TEXT("UHT should parse both module arrays"), GeneratorContents.Contains(TEXT("NativeModuleFunctionAddressModules")) && GeneratorContents.Contains(TEXT("NativeRuntimeLinkedModules")));
		bPassed &= TestRunner->TestTrue(TEXT("Configuration parser should own the canonical method and module tokens"), ConfigurationContents.Contains(TEXT("FunctionBindingMethodKey")) && ConfigurationContents.Contains(TEXT("NativeRuntimeLinkedModulesKey")) && ConfigurationContents.Contains(TEXT("NativeModuleFunctionAddressModulesKey")));
		bPassed &= TestRunner->TestTrue(TEXT("Configuration parser should implement UE append, remove, and clear operations"), ConfigurationContents.Contains(TEXT("case '+'")) && ConfigurationContents.Contains(TEXT("case '-'")) && ConfigurationContents.Contains(TEXT("operation == '!'") ));
		bPassed &= TestRunner->TestTrue(TEXT("Function binding policy should keep callable and target signature decisions explicit"), PolicyContents.Contains(TEXT("IsEligible")) && PolicyContents.Contains(TEXT("GetRuntimeFallbackReason")) && PolicyContents.Contains(TEXT("ClassifyUnsupportedNativeModuleFunctionBindingSignature")));
		bPassed &= TestRunner->TestTrue(TEXT("Configuration resolver should own engine classification and path discovery"), ResolverContents.Contains(TEXT("ClassifyEngineDistribution")) && ResolverContents.Contains(TEXT("TryExtractEngineDirectory")));
		bPassed &= TestRunner->TestTrue(TEXT("Editor should validate target-module source-engine requirements"), EditorContents.Contains(TEXT("ValidateFunctionBindingMethod")) && EditorContents.Contains(TEXT("NativeModuleFunctionAddress")));
		bPassed &= TestRunner->TestFalse(TEXT("Removed compile boolean should not remain active"), BuildCsContents.Contains(TEXT("bCompileAngelscriptModuleBindings")) || GeneratorContents.Contains(TEXT("bCompileAngelscriptModuleBindings")));
		bPassed &= TestRunner->TestFalse(TEXT("Removed JSON profile should not remain active"), GeneratorContents.Contains(TEXT("module-binding-generation-modules.json")));
		TestRunner->TestTrue(TEXT("Function binding compile-option contract should pass"), bPassed);
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptFunctionBindingBridgeContractTests,
	"Angelscript.CppTests.UHTToolResolver.FunctionBindingBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BridgeUsesTheFinalSourceOwnedNames)
	{
		FString HeaderContents;
		if (!ReadFile(FPaths::Combine(GetPluginDirectory(), TEXT("Source/AngelscriptRuntime/FunctionBinding/NativeModuleFunctionBindingBridge.h")), HeaderContents, *TestRunner))
		{
			return;
		}

		static_assert(std::is_standard_layout<FAngelscriptNativeModuleFunctionBinding>::value, "Native module function binding must stay standard-layout.");
		static_assert(std::is_standard_layout<FAngelscriptNativeModuleFunctionBindingView>::value, "Native module function binding view must stay standard-layout.");
		static_assert(sizeof(FAngelscriptNativeModuleFunctionBindingCallFrame) == 48, "Native module function binding call frame ABI changed.");
		static_assert(sizeof(FAngelscriptNativeModuleFunctionBinding) == 32, "Native module function binding ABI changed.");
		static_assert(sizeof(FAngelscriptNativeModuleFunctionBindingView) == 32, "Native module function binding view ABI changed.");

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("Bridge should expose the native module function binding descriptor"), HeaderContents.Contains(TEXT("FAngelscriptNativeModuleFunctionBinding")));
		bPassed &= TestRunner->TestTrue(TEXT("Bridge should expose the source-owned view"), HeaderContents.Contains(TEXT("FAngelscriptNativeModuleFunctionBindingView")));
		bPassed &= TestRunner->TestTrue(TEXT("Bridge should use the final modular feature key"), HeaderContents.Contains(TEXT("AngelscriptNativeModuleFunctionBinding")));
		bPassed &= TestRunner->TestFalse(TEXT("Bridge should not expose Protocol terminology"), HeaderContents.Contains(TEXT("Protocol")));
		TestRunner->TestTrue(TEXT("Native module function binding bridge contract should pass"), bPassed);
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptFunctionBindingOutputContractTests,
	"Angelscript.CppTests.UHTToolResolver.FunctionBindingOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratedFilesAndStatisticsUseFunctionBindingNames)
	{
		FString StatisticsContents;
		FString DiagnosticsContents;
		FString RuntimeShardContents;
		bool bPassed = true;
		bPassed &= ReadFile(FPaths::Combine(GetGeneratedDirectory(), TEXT("AS_FunctionBindingStatistics.json")), StatisticsContents, *TestRunner);
		bPassed &= ReadFile(FPaths::Combine(GetGeneratedDirectory(), TEXT("AS_FunctionBindingDiagnostics.csv")), DiagnosticsContents, *TestRunner);
		bPassed &= ReadFile(FPaths::Combine(GetGeneratedDirectory(), TEXT("AS_FunctionBinding_Engine.gen.cpp")), RuntimeShardContents, *TestRunner);
		if (!bPassed)
		{
			return;
		}

		bPassed &= TestRunner->TestTrue(TEXT("Statistics should identify the selected method"), StatisticsContents.Contains(TEXT("functionBindingMethod")) && StatisticsContents.Contains(TEXT("NativeRuntimeLinked")));
		bPassed &= TestRunner->TestTrue(TEXT("Statistics should expose analyzed-function denominator"), StatisticsContents.Contains(TEXT("totalAnalyzedFunctions")));
		bPassed &= TestRunner->TestTrue(TEXT("Diagnostics should use FunctionBindingCategory"), DiagnosticsContents.Contains(TEXT("FunctionBindingCategory")));
		bPassed &= TestRunner->TestTrue(TEXT("Runtime shard should use the AS_FunctionBinding prefix"), RuntimeShardContents.Contains(TEXT("Bind_AS_FunctionBinding_")));
		bPassed &= TestRunner->TestFalse(TEXT("Diagnostics should not use generic legacy categories"), DiagnosticsContents.Contains(TEXT(",Direct,")) || DiagnosticsContents.Contains(TEXT(",Stub,")) || DiagnosticsContents.Contains(TEXT(",ModuleBinding,")));
		TestRunner->TestTrue(TEXT("Function binding output contract should pass"), bPassed);
	}
};

#endif
