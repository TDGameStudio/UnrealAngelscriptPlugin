#include "AngelscriptOfflineExportService.h"

#include "AngelscriptOfflineSymbolExporter.h"
#include "AngelscriptEngine.h"
#include "Core/AngelscriptSettings.h"
#include "Core/UnrealAngelscriptVersion.h"

#include "GenericPlatform/GenericPlatformProperties.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Modules/ModuleManager.h"

namespace AngelscriptOfflineContract
{
	namespace
	{
		constexpr TCHAR ForkVersion[] =
			TEXT("2.33+selective-2.38");
		constexpr TCHAR CompilerContractVersion[] =
			TEXT("ue-as-standalone-v1");

		FString GetConfiguration()
		{
			switch (FApp::GetBuildConfiguration())
			{
			case EBuildConfiguration::Debug: return TEXT("Debug");
			case EBuildConfiguration::DebugGame: return TEXT("DebugGame");
			case EBuildConfiguration::Development: return TEXT("Development");
			case EBuildConfiguration::Test: return TEXT("Test");
			case EBuildConfiguration::Shipping: return TEXT("Shipping");
			default: return TEXT("Unknown");
			}
		}

		void PopulateLoadedScope(FManifestRecord& Manifest)
		{
			TArray<FModuleStatus> ModuleStatuses;
			FModuleManager::Get().QueryModules(ModuleStatuses);
			for (const FModuleStatus& Status : ModuleStatuses)
			{
				if (Status.bIsLoaded)
				{
					Manifest.LoadedModules.Add(Status.Name);
				}
			}
			Manifest.LoadedModules.Sort();

			for (const TSharedRef<IPlugin>& Plugin :
				IPluginManager::Get().GetEnabledPlugins())
			{
				Manifest.LoadedPlugins.Add(Plugin->GetName());
			}
			Manifest.LoadedPlugins.Sort();
		}

		FManifestRecord BuildManifest(
			const EBundleKind BundleKind,
			const FScopeRecord& SymbolScope,
			const FScopeRecord& AssetScope,
			TArray<FAdapterRecord> Adapters)
		{
			FManifestRecord Manifest;
			Manifest.BundleKind = BundleKind;
			Manifest.ProducerName = TEXT("AngelscriptRuntime");
			Manifest.ProducerVersion =
				UTF8_TO_TCHAR(UNREAL_ANGELSCRIPT_VERSION_STRING);
			Manifest.UnrealVersion =
				FEngineVersion::Current().ToString();
			Manifest.PluginVersion =
				UTF8_TO_TCHAR(UNREAL_ANGELSCRIPT_VERSION_STRING);
			if (const TSharedPtr<IPlugin> Plugin =
				IPluginManager::Get().FindPlugin(TEXT("Angelscript")))
			{
				Manifest.PluginVersion =
					Plugin->GetDescriptor().VersionName;
			}
			Manifest.ForkVersion = ForkVersion;
			Manifest.CompilerContractVersion =
				CompilerContractVersion;
			Manifest.Platform = FPlatformProperties::PlatformName();
			Manifest.Configuration = GetConfiguration();
			Manifest.SymbolScope = SymbolScope;
			Manifest.AssetScope = AssetScope;
			Manifest.Adapters = MoveTemp(Adapters);
			Manifest.RequiredFields = {
				TEXT("manifest.schema"),
				TEXT("manifest.symbolScope"),
				TEXT("records.stableId"),
			};
			Manifest.EngineProperties.Add(
				TEXT("angelscript.fork"),
				ForkVersion);
			Manifest.EngineProperties.Add(
				TEXT("angelscript.float-width"),
				UAngelscriptSettings::Get().bScriptFloatIsFloat64
					? TEXT("64")
					: TEXT("32"));
			Manifest.EngineProperties.Add(
				TEXT("unreal.major"),
				FString::FromInt(
					FEngineVersion::Current().GetMajor()));
			Manifest.EngineProperties.Add(
				TEXT("unreal.minor"),
				FString::FromInt(
					FEngineVersion::Current().GetMinor()));
			Manifest.EngineProperties.Add(
				TEXT("unreal.patch"),
				FString::FromInt(
					FEngineVersion::Current().GetPatch()));
			Manifest.EngineProperties.Add(
				TEXT("unreal.project-name"),
				FApp::GetProjectName());
			Manifest.FeatureFlags.Add(
				TEXT("editor"),
#if WITH_EDITOR
				true
#else
				false
#endif
			);
			Manifest.FeatureFlags.Add(
				TEXT("debugServer"),
#if WITH_AS_DEBUGSERVER
				true
#else
				false
#endif
			);
			Manifest.FeatureFlags.Add(
				TEXT("nativeModuleFunctionAddress"),
#if WITH_ANGELSCRIPT_NATIVE_MODULE_FUNCTION_ADDRESS
				true
#else
				false
#endif
			);
			PopulateLoadedScope(Manifest);
			return Manifest;
		}
	}

	FOfflineExportBuildResult FAngelscriptOfflineExportService::Build(
		FAngelscriptEngine& Engine,
		const FOfflineExportBuildRequest& Request)
	{
		FOfflineExportBuildResult Result;
		asIScriptEngine* const ScriptEngine = Engine.GetScriptEngine();
		if (ScriptEngine == nullptr)
		{
			Result.Error = TEXT("AngelScript engine is unavailable");
			return Result;
		}

		FSymbolExportResult Host =
			FAngelscriptOfflineSymbolExporter::ExportHostSurface(
				*ScriptEngine);
		if (!Host.bSuccess || !Host.SymbolScope.bComplete)
		{
			Result.Error = FString::Printf(
				TEXT("Host-surface export failed: %s"),
				*Host.Error);
			return Result;
		}
		FSymbolExportResult Baseline =
			FAngelscriptOfflineSymbolExporter::ExportScriptBaseline(
				Engine);
		if (!Baseline.bSuccess || !Baseline.SymbolScope.bComplete)
		{
			Result.Error = FString::Printf(
				TEXT("Script-baseline export failed: %s"),
				*Baseline.Error);
			return Result;
		}

		Result.Bundle.OutputDirectory = Request.OutputDirectory;
		Result.Bundle.Symbols = MoveTemp(Host.Symbols);
		Result.Bundle.Symbols.Append(MoveTemp(Baseline.Symbols));
		Result.Bundle.Assets = Request.Assets;
		Result.Bundle.Symbols.Sort([](
			const FSymbolRecord& Left,
			const FSymbolRecord& Right)
		{
			return Left.StableId < Right.StableId;
		});

		for (int32 Index = 0;
			Index < Result.Bundle.Symbols.Num();
			++Index)
		{
			const FSymbolRecord& Symbol = Result.Bundle.Symbols[Index];
			if (Index > 0
				&& Result.Bundle.Symbols[Index - 1].StableId
					== Symbol.StableId)
			{
				Result.Error = FString::Printf(
					TEXT(
						"Host and script layers collide on stable ID '%s'"),
					*Symbol.StableId);
				return Result;
			}
		}
		FScopeRecord SymbolScope;
		SymbolScope.bComplete = true;
		SymbolScope.State =
			TEXT("host-surface+active-script-baseline");
		SymbolScope.Included = Host.SymbolScope.Included;
		SymbolScope.Included.Append(Baseline.SymbolScope.Included);
		SymbolScope.Skipped = Host.SymbolScope.Skipped;
		SymbolScope.Skipped.Append(Baseline.SymbolScope.Skipped);
		SymbolScope.Diagnostics = Host.SymbolScope.Diagnostics;
		SymbolScope.Diagnostics.Append(
			Baseline.SymbolScope.Diagnostics);

		Result.Bundle.Manifest = BuildManifest(
			Request.BundleKind,
			SymbolScope,
			Request.AssetScope,
			MoveTemp(Host.Adapters));
		Result.bSuccess = true;
		return Result;
	}

	FBundleWriteResult FAngelscriptOfflineExportService::Export(
		FAngelscriptEngine& Engine,
		const FOfflineExportBuildRequest& Request)
	{
		FOfflineExportBuildResult Built = Build(Engine, Request);
		if (!Built.bSuccess)
		{
			FBundleWriteResult Result;
			Result.Error = MoveTemp(Built.Error);
			return Result;
		}
		return FAngelscriptOfflineBundleWriter::Write(Built.Bundle);
	}
}
