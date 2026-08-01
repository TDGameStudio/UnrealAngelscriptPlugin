#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	namespace fs = std::filesystem;

	bool Require(bool bCondition, const std::string& Message)
	{
		if (!bCondition)
		{
			std::cerr << Message << '\n';
		}
		return bCondition;
	}

	std::string ReadText(const fs::path& Path)
	{
		std::ifstream Input(Path, std::ios::binary);
		return std::string(
			std::istreambuf_iterator<char>(Input),
			std::istreambuf_iterator<char>());
	}

	bool HasForbiddenCoreDependency(std::string_view Text)
	{
		static constexpr std::string_view Patterns[] = {
			"#include \"CoreMinimal.h\"",
			"#include \"CoreTypes.h\"",
			"#include \"AngelscriptEngine.h\"",
			"#include \"AngelscriptSettings.h\"",
			"#include \"ClassGenerator/ASClass.h\"",
			"#include \"Containers/",
			"#include \"HAL/",
			"FMemory::",
			"FPlatformAtomics::",
			"FAngelscriptEngine::",
			"UASClass::",
			"TArray<",
			"TMap<",
			"TMultiMap<",
		};

		for (std::string_view Pattern : Patterns)
		{
			if (Text.find(Pattern) != std::string_view::npos)
			{
				return true;
			}
		}
		return false;
	}

	bool HasForkPortabilityContamination(std::string_view Text)
	{
		static constexpr std::string_view Patterns[] = {
			"as_portable_containers.h",
			"as_memoryarena.h",
			"as_host_services.h",
			"asCStdArray<",
			"asCHashMap<",
			"asCPair<",
			"asCInlineAllocator<",
			"asCScopedValue<",
			"asCMemoryArena",
			"std::atomic_ref",
			"std::shared_mutex",
			"std::vector<",
		};

		for (const std::string_view Pattern : Patterns)
		{
			if (Text.find(Pattern) != std::string_view::npos)
			{
				return true;
			}
		}
		return false;
	}

	bool HasForbiddenStandaloneHostDependency(std::string_view Text)
	{
		static constexpr std::string_view Patterns[] = {
			"#include \"CoreMinimal.h\"",
			"#include \"CoreTypes.h\"",
			"#include \"UObject/",
			"#include \"Engine/",
			"#include \"Modules/",
			"#include \"HAL/",
			"#include \"Misc/Paths.h\"",
			"#include \"AngelscriptEngine.h\"",
			".generated.h\"",
			"FMemory::",
			"FPlatformProcess::",
			"FAngelscriptEngine::",
			"UObject*",
		};
		for (const std::string_view Pattern : Patterns)
		{
			if (Text.find(Pattern) != std::string_view::npos)
			{
				return true;
			}
		}
		return false;
	}

	bool HasStandaloneBindingContamination(std::string_view Text)
	{
		static constexpr std::string_view Patterns[] = {
			"ANGELSCRIPT_LANGUAGE_STANDALONE",
			"ANGELSCRIPT_STANDALONE",
			"WITH_ANGELSCRIPT_STANDALONE",
			"AS_STANDALONE",
			"ExportStandalone",
			"OfflineSymbolExporter",
			"OfflineBundleWriter",
			"OfflineExportService",
		};
		for (const std::string_view Pattern : Patterns)
		{
			if (Text.find(Pattern) != std::string_view::npos)
			{
				return true;
			}
		}
		return false;
	}

	std::string RemoveCommentOnlyLines(std::string_view Text)
	{
		std::string Result;
		std::size_t Offset = 0;
		while (Offset < Text.size())
		{
			const std::size_t End = Text.find('\n', Offset);
			const std::string_view Line = Text.substr(
				Offset,
				End == std::string_view::npos ? Text.size() - Offset : End - Offset);
			const std::size_t FirstContent = Line.find_first_not_of(" \t");
			if (FirstContent == std::string_view::npos || !Line.substr(FirstContent).starts_with("//"))
			{
				Result.append(Line);
				Result.push_back('\n');
			}
			if (End == std::string_view::npos)
			{
				break;
			}
			Offset = End + 1;
		}
		return Result;
	}
}

int main()
{
	const fs::path PluginRoot = fs::path(ANGELSCRIPT_STANDALONE_SOURCE_ROOT);
	const fs::path ForkRoot = PluginRoot / "Source/AngelscriptRuntime/ThirdParty/angelscript/source";
	const fs::path PublicHeader = PluginRoot / "Source/AngelscriptRuntime/Core/angelscript.h";
	const fs::path StandaloneRoot = PluginRoot / "Standalone";
	const fs::path CompatRoot = StandaloneRoot / "Compat";
	const fs::path StandaloneSourceRoot = StandaloneRoot / "Source";
	const fs::path StandaloneFrontendRoot =
		StandaloneSourceRoot / "Compiler/Frontend";
	const fs::path RuntimeLanguageRoot =
		PluginRoot / "Source/AngelscriptRuntime/Language";
	const fs::path UEPreprocessor =
		PluginRoot / "Source/AngelscriptRuntime/Preprocessor/AngelscriptPreprocessor.cpp";
	const fs::path BindingRoot =
		PluginRoot / "Source/AngelscriptRuntime/Binds";
	const fs::path ClassGeneratorRoot =
		PluginRoot / "Source/AngelscriptRuntime/ClassGenerator";
	const fs::path BuildRoot = fs::path(ANGELSCRIPT_STANDALONE_BUILD_ROOT);

	bool bPassed = true;

	// Detector self-tests are deliberately bad fixtures. UE spellings remain
	// forbidden in the private frontend and ordinary standalone host code, but are
	// expected in the maintained fork and resolved by Standalone/Compat.
	bPassed &= Require(
		HasForbiddenCoreDependency("#include \"CoreMinimal.h\"\n"),
		"language/host detector did not reject a CoreMinimal include");
	bPassed &= Require(
		HasForkPortabilityContamination("std::vector<void*> Values;\n"),
		"fork detector did not reject a mechanical std::vector migration");
	bPassed &= Require(
		HasForkPortabilityContamination("#include \"as_host_services.h\"\n"),
		"fork detector did not reject the obsolete host-service seam");
	bPassed &= Require(
		!HasForkPortabilityContamination("TArray<int> Values;\nFMemory::Malloc(4);\n"),
		"fork detector rejected the preserved UE-spelled boundary");

	const std::vector<std::string> MaintainedSources = {
		"as_atomic.cpp",
		"as_builder.cpp",
		"as_bytecode.cpp",
		"as_callfunc.cpp",
		"as_compiler.cpp",
		"as_configgroup.cpp",
		"as_context.cpp",
		"as_datatype.cpp",
		"as_gc.cpp",
		"as_generic.cpp",
		"as_globalproperty.cpp",
		"as_memory.cpp",
		"as_module.cpp",
		"as_objecttype.cpp",
		"as_outputbuffer.cpp",
		"as_parser.cpp",
		"as_restore.cpp",
		"as_scriptcode.cpp",
		"as_scriptengine.cpp",
		"as_scriptfunction.cpp",
		"as_scriptnode.cpp",
		"as_scriptobject.cpp",
		"as_string.cpp",
		"as_string_util.cpp",
		"as_thread.cpp",
		"as_tokenizer.cpp",
		"as_typeinfo.cpp",
		"as_variablescope.cpp",
	};

	bPassed &= Require(fs::exists(PublicHeader), "maintained public header is missing");
	bPassed &= Require(
		!HasForkPortabilityContamination(RemoveCommentOnlyLines(ReadText(PublicHeader))),
		"maintained public header contains standalone portability contamination");
	bPassed &= Require(
		ReadText(PublicHeader).find("#include \"CoreMinimal.h\"") != std::string::npos,
		"maintained public header no longer preserves its UE Core include");
	bPassed &= Require(
		ReadText(PublicHeader).find("#include \"FunctionCallers.h\"") != std::string::npos,
		"maintained public header no longer preserves FunctionCallers inclusion");

	for (const std::string& SourceName : MaintainedSources)
	{
		const fs::path SourcePath = ForkRoot / SourceName;
		bPassed &= Require(fs::exists(SourcePath), "maintained source is missing: " + SourcePath.string());
		if (fs::exists(SourcePath))
		{
			bPassed &= Require(
				!HasForkPortabilityContamination(RemoveCommentOnlyLines(ReadText(SourcePath))),
				"maintained source contains standalone portability contamination: " + SourcePath.string());
		}
	}

	for (const fs::path& ObsoleteForkFile : {
		ForkRoot / "as_host_services.cpp",
		ForkRoot / "as_host_services.h",
		ForkRoot / "as_memoryarena.h",
		ForkRoot / "as_portable_containers.h"})
	{
		bPassed &= Require(
			!fs::exists(ObsoleteForkFile),
			"obsolete standalone portability file remains in the fork: "
				+ ObsoleteForkFile.string());
	}

	bPassed &= Require(
		!fs::exists(RuntimeLanguageRoot),
		"Runtime-owned Language layer must be removed");
	bPassed &= Require(
		fs::exists(StandaloneFrontendRoot),
		"standalone-private frontend root is missing");
	if (fs::exists(StandaloneFrontendRoot))
	{
		for (const fs::directory_entry& Entry :
			fs::recursive_directory_iterator(StandaloneFrontendRoot))
		{
			if (!Entry.is_regular_file()
				|| (Entry.path().extension() != ".h"
					&& Entry.path().extension() != ".cpp"))
			{
				continue;
			}
			const std::string Text = RemoveCommentOnlyLines(ReadText(Entry.path()));
			bPassed &= Require(
				!HasForbiddenCoreDependency(Text),
				"standalone frontend source has an Unreal dependency: "
					+ Entry.path().string());
			bPassed &= Require(
				Text.find("ANGELSCRIPT_LANGUAGE_STANDALONE") == std::string::npos
					&& Text.find("UEAngelscript::Language") == std::string::npos,
				"standalone frontend retains the removed Language layer identity: "
					+ Entry.path().string());
		}
	}
	const std::string UEPreprocessorText = ReadText(UEPreprocessor);
	bPassed &= Require(
		UEPreprocessorText.find("Language/") == std::string::npos
			&& UEPreprocessorText.find("UEAngelscript::Language") == std::string::npos,
		"UE preprocessor must not delegate to the Standalone frontend");

	const std::string CMakeText = ReadText(StandaloneRoot / "CMakeLists.txt");
	bPassed &= Require(
		CMakeText.find("AngelscriptLanguageCore") == std::string::npos
			&& CMakeText.find("ANGELSCRIPT_LANGUAGE_STANDALONE") == std::string::npos,
		"standalone CMake retains the removed shared LanguageCore target or macro");
	bPassed &= Require(
		CMakeText.find("ANGELSCRIPT_STANDALONE_COMPAT") != std::string::npos
			&& CMakeText.find("${ANGELSCRIPT_STANDALONE_COMPAT}")
				!= std::string::npos,
		"maintained-fork CMake target does not select Standalone/Compat");
	static constexpr std::string_view ForbiddenCMakePatterns[] = {
		"as_callfunc_x86.cpp",
		"as_callfunc_x64_",
		"as_callfunc_arm",
		".asm",
		"Unreal Engine",
		"Engine/Source",
		".generated.h",
	};
	for (std::string_view Pattern : ForbiddenCMakePatterns)
	{
		bPassed &= Require(
			CMakeText.find(Pattern) == std::string::npos,
			"standalone CMake target contains forbidden input: " + std::string(Pattern));
	}

	for (const fs::path& RequiredCompatHeader : {
		CompatRoot / "CoreMinimal.h",
		CompatRoot / "CoreTypes.h",
		CompatRoot / "UECompat.h",
		CompatRoot / "AngelscriptEngine.h",
		CompatRoot / "AngelscriptSettings.h",
		CompatRoot / "ClassGenerator/ASClass.h"})
	{
		bPassed &= Require(
			fs::exists(RequiredCompatHeader),
			"required standalone compatibility header is missing: "
				+ RequiredCompatHeader.string());
	}

	for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(StandaloneRoot))
	{
		if (Entry.path().lexically_relative(StandaloneRoot).begin()->string() == "out")
		{
			continue;
		}
		if (Entry.is_regular_file())
		{
			const std::string FileName = Entry.path().filename().string();
			if (FileName == "CoreMinimal.h" || FileName == "CoreTypes.h")
			{
				const fs::path Relative = Entry.path().lexically_relative(CompatRoot);
				bPassed &= Require(
					!Relative.empty() && *Relative.begin() != "..",
					"UE compatibility entry header escaped Standalone/Compat: "
						+ Entry.path().string());
			}
		}
	}

	for (const fs::directory_entry& Entry
		: fs::recursive_directory_iterator(StandaloneSourceRoot))
	{
		if (!Entry.is_regular_file()
			|| (Entry.path().extension() != ".h"
				&& Entry.path().extension() != ".cpp"))
		{
			continue;
		}
		bPassed &= Require(
			!HasForbiddenStandaloneHostDependency(
				RemoveCommentOnlyLines(ReadText(Entry.path()))),
			"standalone host source has an Unreal dependency: "
				+ Entry.path().string());
	}

	for (const fs::path& ProtectedRoot
		: {BindingRoot, ClassGeneratorRoot})
	{
		for (const fs::directory_entry& Entry
			: fs::recursive_directory_iterator(ProtectedRoot))
		{
			if (!Entry.is_regular_file()
				|| (Entry.path().extension() != ".h"
					&& Entry.path().extension() != ".cpp"))
			{
				continue;
			}
			bPassed &= Require(
				!HasStandaloneBindingContamination(
					RemoveCommentOnlyLines(ReadText(Entry.path()))),
				"existing bindings/ClassGenerator gained a standalone "
				"branch or exporter hook: " + Entry.path().string());
		}
	}

	const fs::path RuntimeBuildFile =
		PluginRoot / "Source/AngelscriptRuntime/AngelscriptRuntime.Build.cs";
	if (fs::exists(RuntimeBuildFile))
	{
		bPassed &= Require(
			ReadText(RuntimeBuildFile).find("Standalone/Compat") == std::string::npos,
			"UBT Runtime module imports the CMake-only compatibility facade");
	}
	const fs::path RuntimeModule =
		PluginRoot / "Source/AngelscriptRuntime/Core/AngelscriptRuntimeModule.cpp";
	bPassed &= Require(
		ReadText(RuntimeModule).find("AngelscriptSDKHostServices") == std::string::npos,
		"UE Runtime still installs the standalone SDK host-service table");
	bPassed &= Require(
		!fs::exists(
			PluginRoot
			/ "Source/AngelscriptRuntime/Core/AngelscriptSDKHostServices.h")
			&& !fs::exists(
				PluginRoot
				/ "Source/AngelscriptRuntime/Core/AngelscriptSDKHostServices.cpp"),
		"obsolete UE Runtime SDK host-service adapter remains present");

	static constexpr std::string_view ForbiddenNativeLibrarySurface[] = {
		"fopen(",
		"CreateProcess",
		"ShellExecute",
		"LoadLibrary",
		"WinHttp",
		"socket(",
		"std::filesystem",
		"system(",
	};
	for (const fs::path& NativeSurfaceRoot
		: {StandaloneRoot / "Source/StdLib",
			StandaloneRoot / "ThirdParty/AngelScriptAddons"})
	{
		for (const fs::directory_entry& Entry
			: fs::recursive_directory_iterator(NativeSurfaceRoot))
		{
			if (!Entry.is_regular_file()
				|| (Entry.path().extension() != ".h"
					&& Entry.path().extension() != ".cpp"))
			{
				continue;
			}
			const std::string Text =
				RemoveCommentOnlyLines(ReadText(Entry.path()));
			for (const std::string_view Pattern
				: ForbiddenNativeLibrarySurface)
			{
				bPassed &= Require(
					Text.find(Pattern) == std::string::npos,
					"native standard library exposes a forbidden host "
					"surface '" + std::string(Pattern) + "': "
						+ Entry.path().string());
			}
		}
	}

	const fs::path ProjectFile = BuildRoot / "AngelscriptMaintainedFork.vcxproj";
	bPassed &= Require(fs::exists(ProjectFile), "generated maintained-fork project is missing");
	if (fs::exists(ProjectFile))
	{
		const std::string ProjectText = ReadText(ProjectFile);
		bPassed &= Require(
			ProjectText.find("Unreal Engine") == std::string::npos &&
			ProjectText.find("Engine\\\\Source") == std::string::npos &&
			ProjectText.find(".generated.h") == std::string::npos,
			"generated compile/link target contains an Unreal or generated-code path");
		bPassed &= Require(
			ProjectText.find("Standalone\\Compat") != std::string::npos
				|| ProjectText.find("Standalone/Compat") != std::string::npos,
			"generated maintained-fork target does not contain the compatibility include path");
	}
	const fs::path SolutionFile = BuildRoot / "AngelscriptStandalone.sln";
	bPassed &= Require(
		fs::exists(SolutionFile),
		"generated Standalone solution is missing");
	if (fs::exists(SolutionFile))
	{
		const std::string SolutionText = ReadText(SolutionFile);
		bPassed &= Require(
			SolutionText.find("AngelscriptLanguageCore") == std::string::npos,
			"generated solution still exposes a LanguageCore target");
	}
	const fs::path HostProjectFile = BuildRoot / "AngelscriptStandaloneHost.vcxproj";
	bPassed &= Require(
		fs::exists(HostProjectFile),
		"generated Standalone Host project is missing");
	if (fs::exists(HostProjectFile))
	{
		const std::string ProjectText = ReadText(HostProjectFile);
		bPassed &= Require(
			ProjectText.find("Unreal Engine") == std::string::npos
				&& ProjectText.find("Engine\\\\Source") == std::string::npos
				&& ProjectText.find(".generated.h") == std::string::npos
				&& ProjectText.find("AngelscriptRuntime\\Language") == std::string::npos
				&& ProjectText.find("AngelscriptRuntime/Language") == std::string::npos,
			"generated Standalone Host target contains Unreal or Runtime Language paths");
	}

	return bPassed ? 0 : 1;
}
